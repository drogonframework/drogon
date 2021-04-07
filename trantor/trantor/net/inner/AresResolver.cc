// Copyright 2016, Tao An.  All rights reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Tao An

#include "AresResolver.h"
#include <trantor/net/Channel.h>
#include <ares.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <arpa/inet.h>  // inet_ntop
#include <netinet/in.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

using namespace trantor;
using namespace std::placeholders;

namespace
{
double getSeconds(struct timeval* tv)
{
    if (tv)
        return double(tv->tv_sec) + double(tv->tv_usec) / 1000000.0;
    else
        return -1.0;
}

const char* getSocketType(int type)
{
    if (type == SOCK_DGRAM)
        return "UDP";
    else if (type == SOCK_STREAM)
        return "TCP";
    else
        return "Unknown";
}

}  // namespace

bool Resolver::isCAresUsed()
{
    return true;
}

AresResolver::LibraryInitializer::LibraryInitializer()
{
    ares_library_init(ARES_LIB_INIT_ALL);
}
AresResolver::LibraryInitializer::~LibraryInitializer()
{
    ares_library_cleanup();
}

AresResolver::LibraryInitializer AresResolver::libraryInitializer_;

std::shared_ptr<Resolver> Resolver::newResolver(trantor::EventLoop* loop,
                                                size_t timeout)
{
    return std::make_shared<AresResolver>(loop, timeout);
}

AresResolver::AresResolver(EventLoop* loop, size_t timeout)
    : loop_(loop), timeout_(timeout)
{
    if (!loop)
    {
        loop_ = getLoop();
    }
}
void AresResolver::init()
{
    if (!ctx_)
    {
        struct ares_options options;
        int optmask = ARES_OPT_FLAGS;
        options.flags = ARES_FLAG_NOCHECKRESP;
        options.flags |= ARES_FLAG_STAYOPEN;
        options.flags |= ARES_FLAG_IGNTC;  // UDP only
        optmask |= ARES_OPT_SOCK_STATE_CB;
        options.sock_state_cb = &AresResolver::ares_sock_statecallback_;
        options.sock_state_cb_data = this;
        optmask |= ARES_OPT_TIMEOUT;
        options.timeout = 2;
        // optmask |= ARES_OPT_LOOKUPS;
        // options.lookups = lookups;

        int status = ares_init_options(&ctx_, &options, optmask);
        if (status != ARES_SUCCESS)
        {
            assert(0);
        }
        ares_set_socket_callback(ctx_,
                                 &AresResolver::ares_sock_createcallback_,
                                 this);
    }
}
AresResolver::~AresResolver()
{
    if (ctx_)
        ares_destroy(ctx_);
}

void AresResolver::resolveInLoop(const std::string& hostname,
                                 const Callback& cb)
{
    loop_->assertInLoopThread();
#ifdef _WIN32
    if (hostname == "localhost")
    {
        const static trantor::InetAddress localhost_{"127.0.0.1", 0};
        cb(localhost_);
        return;
    }
#endif
    init();
    QueryData* queryData = new QueryData(this, cb, hostname);
    ares_gethostbyname(ctx_,
                       hostname.c_str(),
                       AF_INET,
                       &AresResolver::ares_hostcallback_,
                       queryData);
    struct timeval tv;
    struct timeval* tvp = ares_timeout(ctx_, NULL, &tv);
    double timeout = getSeconds(tvp);
    // LOG_DEBUG << "timeout " << timeout << " active " << timerActive_;
    if (!timerActive_ && timeout >= 0.0)
    {
        loop_->runAfter(timeout,
                        std::bind(&AresResolver::onTimer, shared_from_this()));
        timerActive_ = true;
    }
    return;
}

void AresResolver::onRead(int sockfd)
{
    ares_process_fd(ctx_, sockfd, ARES_SOCKET_BAD);
}

void AresResolver::onTimer()
{
    assert(timerActive_ == true);
    ares_process_fd(ctx_, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    struct timeval tv;
    struct timeval* tvp = ares_timeout(ctx_, NULL, &tv);
    double timeout = getSeconds(tvp);

    if (timeout < 0)
    {
        timerActive_ = false;
    }
    else
    {
        loop_->runAfter(timeout,
                        std::bind(&AresResolver::onTimer, shared_from_this()));
    }
}

void AresResolver::onQueryResult(int status,
                                 struct hostent* result,
                                 const std::string& hostname,
                                 const Callback& callback)
{
    LOG_TRACE << "onQueryResult " << status;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    if (result)
    {
        addr.sin_addr = *reinterpret_cast<in_addr*>(result->h_addr);
    }
    InetAddress inet(addr);
    {
        std::lock_guard<std::mutex> lock(globalMutex());
        auto& addrItem = globalCache()[hostname];
        addrItem.first = addr.sin_addr;
        addrItem.second = trantor::Date::date();
    }
    callback(inet);
}

void AresResolver::onSockCreate(int sockfd, int type)
{
    (void)type;
    loop_->assertInLoopThread();
    assert(channels_.find(sockfd) == channels_.end());
    Channel* channel = new Channel(loop_, sockfd);
    channel->setReadCallback(std::bind(&AresResolver::onRead, this, sockfd));
    channel->enableReading();
    channels_[sockfd].reset(channel);
}

void AresResolver::onSockStateChange(int sockfd, bool read, bool write)
{
    (void)write;
    loop_->assertInLoopThread();
    ChannelList::iterator it = channels_.find(sockfd);
    assert(it != channels_.end());
    if (read)
    {
        // update
        // if (write) { } else { }
    }
    else
    {
        // remove
        it->second->disableAll();
        it->second->remove();
        channels_.erase(it);
    }
}

void AresResolver::ares_hostcallback_(void* data,
                                      int status,
                                      int timeouts,
                                      struct hostent* hostent)
{
    (void)timeouts;
    QueryData* query = static_cast<QueryData*>(data);

    query->owner_->onQueryResult(status,
                                 hostent,
                                 query->hostname_,
                                 query->callback_);
    delete query;
}

#ifdef _WIN32
int AresResolver::ares_sock_createcallback_(SOCKET sockfd, int type, void* data)
#else
int AresResolver::ares_sock_createcallback_(int sockfd, int type, void* data)
#endif
{
    LOG_TRACE << "sockfd=" << sockfd << " type=" << getSocketType(type);
    static_cast<AresResolver*>(data)->onSockCreate(sockfd, type);
    return 0;
}

void AresResolver::ares_sock_statecallback_(void* data,
#ifdef _WIN32
                                            SOCKET sockfd,
#else
                                            int sockfd,
#endif
                                            int read,
                                            int write)
{
    LOG_TRACE << "sockfd=" << sockfd << " read=" << read << " write=" << write;
    static_cast<AresResolver*>(data)->onSockStateChange(sockfd, read, write);
}
