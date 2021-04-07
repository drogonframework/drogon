// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// Taken from muduo and modified by an tao

#include <trantor/net/TcpClient.h>

#include <trantor/utils/Logger.h>
#include "Connector.h"
#include "inner/TcpConnectionImpl.h"
#include <trantor/net/EventLoop.h>

#include <functional>
#include <algorithm>

#include "Socket.h"

#include <stdio.h>  // snprintf

using namespace trantor;
using namespace std::placeholders;

namespace trantor
{
// void removeConnector(const ConnectorPtr &)
// {
//     // connector->
// }
#ifndef _WIN32
TcpClient::IgnoreSigPipe TcpClient::initObj;
#endif

static void defaultConnectionCallback(const TcpConnectionPtr &conn)
{
    LOG_TRACE << conn->localAddr().toIpPort() << " -> "
              << conn->peerAddr().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
    // do not call conn->forceClose(), because some users want to register
    // message callback only.
}

static void defaultMessageCallback(const TcpConnectionPtr &, MsgBuffer *buf)
{
    buf->retrieveAll();
}

}  // namespace trantor

TcpClient::TcpClient(EventLoop *loop,
                     const InetAddress &serverAddr,
                     const std::string &nameArg)
    : loop_(loop),
      connector_(new Connector(loop, serverAddr, false)),
      name_(nameArg),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      retry_(false),
      connect_(true)
{
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, _1));
    connector_->setErrorCallback([this]() {
        if (connectionErrorCallback_)
        {
            connectionErrorCallback_();
        }
    });
    LOG_TRACE << "TcpClient::TcpClient[" << name_ << "] - connector ";
}

TcpClient::~TcpClient()
{
    LOG_TRACE << "TcpClient::~TcpClient[" << name_ << "] - connector ";
    TcpConnectionImplPtr conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn = std::dynamic_pointer_cast<TcpConnectionImpl>(connection_);
    }
    if (conn)
    {
        assert(loop_ == conn->getLoop());
        // TODO: not 100% safe, if we are in different thread
        auto loop = loop_;
        loop_->runInLoop([conn, loop]() {
            conn->setCloseCallback([loop](const TcpConnectionPtr &connPtr) {
                loop->queueInLoop([connPtr]() {
                    static_cast<TcpConnectionImpl *>(connPtr.get())
                        ->connectDestroyed();
                });
            });
        });
        conn->forceClose();
    }
    else
    {
        /// TODO need test in this condition
        connector_->stop();
    }
}

void TcpClient::connect()
{
    // TODO: check state
    LOG_TRACE << "TcpClient::connect[" << name_ << "] - connecting to "
              << connector_->serverAddress().toIpPort();
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect()
{
    connect_ = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_)
        {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
    loop_->assertInLoopThread();
    InetAddress peerAddr(Socket::getPeerAddr(sockfd));
    InetAddress localAddr(Socket::getLocalAddr(sockfd));
    // TODO poll with zero timeout to double confirm the new connection
    // TODO use make_shared if necessary
    std::shared_ptr<TcpConnectionImpl> conn;
    if (sslCtxPtr_)
    {
#ifdef USE_OPENSSL
        conn = std::make_shared<TcpConnectionImpl>(loop_,
                                                   sockfd,
                                                   localAddr,
                                                   peerAddr,
                                                   sslCtxPtr_,
                                                   false,
                                                   validateCert_,
                                                   SSLHostName_);
#else
        LOG_FATAL << "OpenSSL is not found in your system!";
        abort();
#endif
    }
    else
    {
        conn = std::make_shared<TcpConnectionImpl>(loop_,
                                                   sockfd,
                                                   localAddr,
                                                   peerAddr);
    }
    conn->setConnectionCallback(connectionCallback_);
    conn->setRecvMsgCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, _1));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }
    conn->setSSLErrorCallback([this](SSLError err) {
        if (sslErrorCallback_)
        {
            sslErrorCallback_(err);
        }
    });
    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        assert(connection_ == conn);
        connection_.reset();
    }

    loop_->queueInLoop(
        std::bind(&TcpConnectionImpl::connectDestroyed,
                  std::dynamic_pointer_cast<TcpConnectionImpl>(conn)));
    if (retry_ && connect_)
    {
        LOG_TRACE << "TcpClient::connect[" << name_ << "] - Reconnecting to "
                  << connector_->serverAddress().toIpPort();
        connector_->restart();
    }
}

void TcpClient::enableSSL(bool useOldTLS,
                          bool validateCert,
                          std::string hostname)
{
#ifdef USE_OPENSSL
    /* Create a new OpenSSL context */
    sslCtxPtr_ = newSSLContext(useOldTLS, validateCert);
    validateCert_ = validateCert;
    if (!hostname.empty())
    {
        std::transform(hostname.begin(),
                       hostname.end(),
                       hostname.begin(),
                       tolower);
        SSLHostName_ = std::move(hostname);
    }

#else
    LOG_FATAL << "OpenSSL is not found in your system!";
    abort();
#endif
}
