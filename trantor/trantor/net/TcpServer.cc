/**
 *
 *  @file TcpServer.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/trantor
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *  Trantor
 *
 */

#include "Acceptor.h"
#include "inner/TcpConnectionImpl.h"
#include <trantor/net/TcpServer.h>
#include <trantor/utils/Logger.h>
#include <functional>
#include <vector>
using namespace trantor;
using namespace std::placeholders;

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &address,
                     const std::string &name,
                     bool reUseAddr,
                     bool reUsePort)
    : loop_(loop),
      acceptorPtr_(new Acceptor(loop, address, reUseAddr, reUsePort)),
      serverName_(name),
      recvMessageCallback_([](const TcpConnectionPtr &, MsgBuffer *buffer) {
          LOG_ERROR << "unhandled recv message [" << buffer->readableBytes()
                    << " bytes]";
          buffer->retrieveAll();
      })
{
    acceptorPtr_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
    // loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << serverName_ << "] destructing";
}

void TcpServer::newConnection(int sockfd, const InetAddress &peer)
{
    LOG_TRACE << "new connection:fd=" << sockfd
              << " address=" << peer.toIpPort();
    // test code for blocking or nonblocking
    //    std::vector<char> str(1024*1024*100);
    //    for(int i=0;i<str.size();i++)
    //        str[i]='A';
    //    LOG_TRACE<<"vector size:"<<str.size();
    //    size_t n=write(sockfd,&str[0],str.size());
    //    LOG_TRACE<<"write "<<n<<" bytes";
    loop_->assertInLoopThread();
    EventLoop *ioLoop = NULL;
    if (loopPoolPtr_ && loopPoolPtr_->size() > 0)
    {
        ioLoop = loopPoolPtr_->getNextLoop();
    }
    if (ioLoop == NULL)
        ioLoop = loop_;
    std::shared_ptr<TcpConnectionImpl> newPtr;
    if (sslCtxPtr_)
    {
#ifdef USE_OPENSSL
        newPtr = std::make_shared<TcpConnectionImpl>(
            ioLoop,
            sockfd,
            InetAddress(Socket::getLocalAddr(sockfd)),
            peer,
            sslCtxPtr_);
#else
        LOG_FATAL << "OpenSSL is not found in your system!";
        abort();
#endif
    }
    else
    {
        newPtr = std::make_shared<TcpConnectionImpl>(
            ioLoop, sockfd, InetAddress(Socket::getLocalAddr(sockfd)), peer);
    }

    if (idleTimeout_ > 0)
    {
        assert(timingWheelMap_[ioLoop]);
        newPtr->enableKickingOff(idleTimeout_, timingWheelMap_[ioLoop]);
    }
    newPtr->setRecvMsgCallback(recvMessageCallback_);

    newPtr->setConnectionCallback(
        [this](const TcpConnectionPtr &connectionPtr) {
            if (connectionCallback_)
                connectionCallback_(connectionPtr);
        });
    newPtr->setWriteCompleteCallback(
        [this](const TcpConnectionPtr &connectionPtr) {
            if (writeCompleteCallback_)
                writeCompleteCallback_(connectionPtr);
        });
    newPtr->setCloseCallback(std::bind(&TcpServer::connectionClosed, this, _1));
    connSet_.insert(newPtr);
    newPtr->connectEstablished();
}

void TcpServer::start()
{
    loop_->runInLoop([this]() {
        assert(!started_);
        started_ = true;
        if (idleTimeout_ > 0)
        {
            timingWheelMap_[loop_] =
                std::make_shared<TimingWheel>(loop_,
                                              idleTimeout_,
                                              1.0F,
                                              idleTimeout_ < 500
                                                  ? idleTimeout_ + 1
                                                  : 100);
            if (loopPoolPtr_)
            {
                auto loopNum = loopPoolPtr_->size();
                while (loopNum > 0)
                {
                    // LOG_TRACE << "new Wheel loopNum=" << loopNum;
                    auto poolLoop = loopPoolPtr_->getNextLoop();
                    timingWheelMap_[poolLoop] =
                        std::make_shared<TimingWheel>(poolLoop,
                                                      idleTimeout_,
                                                      1.0F,
                                                      idleTimeout_ < 500
                                                          ? idleTimeout_ + 1
                                                          : 100);
                    --loopNum;
                }
            }
        }
        LOG_TRACE << "map size=" << timingWheelMap_.size();
        acceptorPtr_->listen();
    });
}
void TcpServer::stop()
{
    loop_->runInLoop([this]() { acceptorPtr_.reset(); });
    for (auto connection : connSet_)
    {
        connection->forceClose();
    }
    loopPoolPtr_.reset();
    for (auto &iter : timingWheelMap_)
    {
        std::promise<int> pro;
        auto f = pro.get_future();
        iter.second->getLoop()->runInLoop([&iter, &pro]() mutable {
            iter.second.reset();
            pro.set_value(1);
        });
        f.get();
    }
}
void TcpServer::connectionClosed(const TcpConnectionPtr &connectionPtr)
{
    LOG_TRACE << "connectionClosed";
    // loop_->assertInLoopThread();
    loop_->runInLoop([this, connectionPtr]() {
        size_t n = connSet_.erase(connectionPtr);
        (void)n;
        assert(n == 1);
    });

    static_cast<TcpConnectionImpl *>(connectionPtr.get())->connectDestroyed();
}

const std::string TcpServer::ipPort() const
{
    return acceptorPtr_->addr().toIpPort();
}

const trantor::InetAddress &TcpServer::address() const
{
    return acceptorPtr_->addr();
}

void TcpServer::enableSSL(const std::string &certPath,
                          const std::string &keyPath,
                          bool useOldTLS)
{
#ifdef USE_OPENSSL
    /* Create a new OpenSSL context */
    sslCtxPtr_ = newSSLServerContext(certPath, keyPath, useOldTLS);
#else
    LOG_FATAL << "OpenSSL is not found in your system!";
    abort();
#endif
}
