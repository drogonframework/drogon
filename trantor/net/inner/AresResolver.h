// Copyright 2016, Tao An.  All rights reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Tao An

#pragma once
#include <trantor/net/Resolver.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoopThread.h>
#include <map>
#include <memory>
#include <string.h>

extern "C"
{
    struct hostent;
    struct ares_channeldata;
    using ares_channel = struct ares_channeldata*;
}
namespace trantor
{
class AresResolver : public Resolver,
                     public NonCopyable,
                     public std::enable_shared_from_this<AresResolver>
{
  public:
    AresResolver(trantor::EventLoop* loop, size_t timeout);
    ~AresResolver();

    virtual void resolve(const std::string& hostname,
                         const Callback& cb) override
    {
        bool cached = false;
        InetAddress inet;
        {
            std::lock_guard<std::mutex> lock(globalMutex());
            auto iter = globalCache().find(hostname);
            if (iter != globalCache().end())
            {
                auto& cachedAddr = iter->second;
                if (timeout_ == 0 ||
                    cachedAddr.second.after(timeout_) > trantor::Date::date())
                {
                    struct sockaddr_in addr;
                    memset(&addr, 0, sizeof addr);
                    addr.sin_family = AF_INET;
                    addr.sin_port = 0;
                    addr.sin_addr = cachedAddr.first;
                    inet = InetAddress(addr);
                    cached = true;
                }
            }
        }
        if (cached)
        {
            cb(inet);
            return;
        }
        if (loop_->isInLoopThread())
        {
            resolveInLoop(hostname, cb);
        }
        else
        {
            loop_->queueInLoop([thisPtr = shared_from_this(), hostname, cb]() {
                thisPtr->resolveInLoop(hostname, cb);
            });
        }
    }

  private:
    struct QueryData
    {
        AresResolver* owner_;
        Callback callback_;
        std::string hostname_;
        QueryData(AresResolver* o,
                  const Callback& cb,
                  const std::string& hostname)
            : owner_(o), callback_(cb), hostname_(hostname)
        {
        }
    };
    void resolveInLoop(const std::string& hostname, const Callback& cb);
    void init();
    trantor::EventLoop* loop_;
    ares_channel ctx_{nullptr};
    bool timerActive_{false};
    using ChannelList = std::map<int, std::unique_ptr<trantor::Channel>>;
    ChannelList channels_;
    static std::unordered_map<std::string,
                              std::pair<struct in_addr, trantor::Date>>&
    globalCache()
    {
        static std::unordered_map<std::string,
                                  std::pair<struct in_addr, trantor::Date>>
            dnsCache;
        return dnsCache;
    }
    static std::mutex& globalMutex()
    {
        static std::mutex mutex_;
        return mutex_;
    }
    static EventLoop* getLoop()
    {
        static EventLoopThread loopThread;
        loopThread.run();
        return loopThread.getLoop();
    }
    const size_t timeout_{60};

    void onRead(int sockfd);
    void onTimer();
    void onQueryResult(int status,
                       struct hostent* result,
                       const std::string& hostname,
                       const Callback& callback);
    void onSockCreate(int sockfd, int type);
    void onSockStateChange(int sockfd, bool read, bool write);

    static void ares_hostcallback_(void* data,
                                   int status,
                                   int timeouts,
                                   struct hostent* hostent);
#ifdef _WIN32
    static int ares_sock_createcallback_(SOCKET sockfd, int type, void* data);
#else
    static int ares_sock_createcallback_(int sockfd, int type, void* data);
#endif
    static void ares_sock_statecallback_(void* data,
#ifdef _WIN32
                                         SOCKET sockfd,
#else
                                         int sockfd,
#endif
                                         int read,
                                         int write);
    struct LibraryInitializer
    {
        LibraryInitializer();
        ~LibraryInitializer();
    };
    static LibraryInitializer libraryInitializer_;
};
}  // namespace trantor
