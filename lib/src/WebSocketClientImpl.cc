/**
 *
 *  @file WebSocketClientImpl.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "WebSocketClientImpl.h"
#include "HttpResponseImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseParser.h"
#include "HttpUtils.h"
#include "WebSocketConnectionImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/utils/Utilities.h>
#include <drogon/config.h>
#include <trantor/net/InetAddress.h>
#ifdef OpenSSL_FOUND
#include <openssl/sha.h>
#else
#include "ssl_funcs/Sha1.h"
#endif

using namespace drogon;
using namespace trantor;

WebSocketClientImpl::~WebSocketClientImpl()
{
}
WebSocketConnectionPtr WebSocketClientImpl::getConnection()
{
    return websockConnPtr_;
}
void WebSocketClientImpl::createTcpClient()
{
    LOG_TRACE << "New TcpClient," << serverAddr_.toIpPort();
    tcpClientPtr_ =
        std::make_shared<trantor::TcpClient>(loop_, serverAddr_, "httpClient");
    if (useSSL_)
    {
        tcpClientPtr_->enableSSL(useOldTLS_);
    }
    auto thisPtr = shared_from_this();
    std::weak_ptr<WebSocketClientImpl> weakPtr = thisPtr;

    tcpClientPtr_->setConnectionCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            if (connPtr->connected())
            {
                connPtr->setContext(std::make_shared<HttpResponseParser>());
                // send request;
                LOG_TRACE << "Connection established!";
                thisPtr->sendReq(connPtr);
            }
            else
            {
                LOG_TRACE << "connection disconnect";
                thisPtr->connectionClosedCallback_(thisPtr);
                thisPtr->websockConnPtr_.reset();
                thisPtr->loop_->runAfter(1.0,
                                         [thisPtr]() { thisPtr->reconnect(); });
            }
        });
    tcpClientPtr_->setConnectionErrorCallback([weakPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        // can't connect to server
        thisPtr->requestCallback_(ReqResult::NetworkFailure, nullptr, thisPtr);
        thisPtr->loop_->runAfter(1.0, [thisPtr]() { thisPtr->reconnect(); });
    });
    tcpClientPtr_->setMessageCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr,
                  trantor::MsgBuffer *msg) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                thisPtr->onRecvMessage(connPtr, msg);
            }
        });
    tcpClientPtr_->connect();
}
void WebSocketClientImpl::connectToServerInLoop()
{
    loop_->assertInLoopThread();
    upgradeRequest_->addHeader("Connection", "Upgrade");
    upgradeRequest_->addHeader("Upgrade", "websocket");
    auto randStr = utils::genRandomString(16);
    wsKey_ = utils::base64Encode((const unsigned char *)randStr.data(),
                                 (unsigned int)randStr.length());

    auto wsKey = wsKey_;
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()),
         wsKey.length(),
         accKey);
    wsAccept_ = utils::base64Encode(accKey, SHA_DIGEST_LENGTH);

    upgradeRequest_->addHeader("Sec-WebSocket-Key", wsKey_);
    // upgradeRequest_->addHeader("Sec-WebSocket-Version","13");

    assert(!tcpClientPtr_);

    bool hasIpv6Address = false;
    if (serverAddr_.isIpV6())
    {
        auto ipaddr = serverAddr_.ip6NetEndian();
        for (int i = 0; i < 4; ++i)
        {
            if (ipaddr[i] != 0)
            {
                hasIpv6Address = true;
                break;
            }
        }
    }

    if (serverAddr_.ipNetEndian() == 0 && !hasIpv6Address && !domain_.empty() &&
        serverAddr_.portNetEndian() != 0)
    {
        if (!resolver_)
        {
            resolver_ = trantor::Resolver::newResolver(loop_);
        }
        resolver_->resolve(
            domain_,
            [thisPtr = shared_from_this(),
             hasIpv6Address](const trantor::InetAddress &addr) {
                thisPtr->loop_->runInLoop([thisPtr, addr, hasIpv6Address]() {
                    auto port = thisPtr->serverAddr_.portNetEndian();
                    thisPtr->serverAddr_ = addr;
                    thisPtr->serverAddr_.setPortNetEndian(port);
                    LOG_TRACE << "dns:domain=" << thisPtr->domain_
                              << ";ip=" << thisPtr->serverAddr_.toIp();
                    if ((thisPtr->serverAddr_.ipNetEndian() != 0 ||
                         hasIpv6Address) &&
                        thisPtr->serverAddr_.portNetEndian() != 0)
                    {
                        thisPtr->createTcpClient();
                    }
                    else
                    {
                        thisPtr->requestCallback_(ReqResult::BadServerAddress,
                                                  nullptr,
                                                  thisPtr);
                        return;
                    }
                });
            });
        return;
    }

    if ((serverAddr_.ipNetEndian() != 0 || hasIpv6Address) &&
        serverAddr_.portNetEndian() != 0)
    {
        createTcpClient();
    }
    else
    {
        requestCallback_(ReqResult::BadServerAddress,
                         nullptr,
                         shared_from_this());
        return;
    }
}

void WebSocketClientImpl::onRecvWsMessage(
    const trantor::TcpConnectionPtr &connPtr,
    trantor::MsgBuffer *msgBuffer)
{
    assert(websockConnPtr_);
    websockConnPtr_->onNewMessage(connPtr, msgBuffer);
}

void WebSocketClientImpl::onRecvMessage(
    const trantor::TcpConnectionPtr &connPtr,
    trantor::MsgBuffer *msgBuffer)
{
    if (upgraded_)
    {
        onRecvWsMessage(connPtr, msgBuffer);
        return;
    }
    auto responseParser = connPtr->getContext<HttpResponseParser>();

    // LOG_TRACE << "###:" << msg->readableBytes();

    if (!responseParser->parseResponse(msgBuffer))
    {
        requestCallback_(ReqResult::BadResponse, nullptr, shared_from_this());
        connPtr->shutdown();
        websockConnPtr_.reset();
        tcpClientPtr_.reset();
        return;
    }

    if (responseParser->gotAll())
    {
        auto resp = responseParser->responseImpl();
        responseParser->reset();
        auto acceptStr = resp->getHeaderBy("sec-websocket-accept");

        if (resp->statusCode() != k101SwitchingProtocols ||
            acceptStr != wsAccept_)
        {
            requestCallback_(ReqResult::BadResponse,
                             nullptr,
                             shared_from_this());
            connPtr->shutdown();
            websockConnPtr_.reset();
            tcpClientPtr_.reset();
            return;
        }

        auto &type = resp->getHeaderBy("content-type");
        if (type.find("application/json") != std::string::npos)
        {
            resp->parseJson();
        }

        if (resp->getHeaderBy("content-encoding") == "gzip")
        {
            resp->gunzip();
        }

        upgraded_ = true;
        websockConnPtr_ =
            std::make_shared<WebSocketConnectionImpl>(connPtr, false);
        auto thisPtr = shared_from_this();
        websockConnPtr_->setMessageCallback(
            [thisPtr](std::string &&message,
                      const WebSocketConnectionImplPtr &,
                      const WebSocketMessageType &type) {
                thisPtr->messageCallback_(std::move(message), thisPtr, type);
            });
        requestCallback_(ReqResult::Ok, resp, shared_from_this());
        if (msgBuffer->readableBytes() > 0)
        {
            onRecvWsMessage(connPtr, msgBuffer);
        }
    }
    else
    {
        return;
    }
}

void WebSocketClientImpl::reconnect()
{
    tcpClientPtr_.reset();
    websockConnPtr_.reset();
    upgraded_ = false;
    connectToServerInLoop();
}

WebSocketClientImpl::WebSocketClientImpl(trantor::EventLoop *loop,
                                         const trantor::InetAddress &addr,
                                         bool useSSL,
                                         bool useOldTLS)
    : loop_(loop), serverAddr_(addr), useSSL_(useSSL), useOldTLS_(useOldTLS)
{
}

WebSocketClientImpl::WebSocketClientImpl(trantor::EventLoop *loop,
                                         const std::string &hostString,
                                         bool useOldTLS)
    : loop_(loop), useOldTLS_(useOldTLS)
{
    auto lowerHost = hostString;
    std::transform(lowerHost.begin(),
                   lowerHost.end(),
                   lowerHost.begin(),
                   tolower);
    if (lowerHost.find("wss://") != std::string::npos)
    {
        useSSL_ = true;
        lowerHost = lowerHost.substr(6);
    }
    else if (lowerHost.find("ws://") != std::string::npos)
    {
        useSSL_ = false;
        lowerHost = lowerHost.substr(5);
    }
    else
    {
        return;
    }
    auto pos = lowerHost.find(']');
    if (lowerHost[0] == '[' && pos != std::string::npos)
    {
        // ipv6
        domain_ = lowerHost.substr(1, pos - 1);
        if (lowerHost[pos + 1] == ':')
        {
            auto portStr = lowerHost.substr(pos + 2);
            pos = portStr.find('/');
            if (pos != std::string::npos)
            {
                portStr = portStr.substr(0, pos);
            }
            auto port = atoi(portStr.c_str());
            if (port > 0 && port < 65536)
            {
                serverAddr_ = InetAddress(domain_, port, true);
            }
        }
        else
        {
            if (useSSL_)
            {
                serverAddr_ = InetAddress(domain_, 443, true);
            }
            else
            {
                serverAddr_ = InetAddress(domain_, 80, true);
            }
        }
    }
    else
    {
        auto pos = lowerHost.find(':');
        if (pos != std::string::npos)
        {
            domain_ = lowerHost.substr(0, pos);
            auto portStr = lowerHost.substr(pos + 1);
            pos = portStr.find('/');
            if (pos != std::string::npos)
            {
                portStr = portStr.substr(0, pos);
            }
            auto port = atoi(portStr.c_str());
            if (port > 0 && port < 65536)
            {
                serverAddr_ = InetAddress(domain_, port);
            }
        }
        else
        {
            domain_ = lowerHost;
            pos = domain_.find('/');
            if (pos != std::string::npos)
            {
                domain_ = domain_.substr(0, pos);
            }
            if (useSSL_)
            {
                serverAddr_ = InetAddress(domain_, 443);
            }
            else
            {
                serverAddr_ = InetAddress(domain_, 80);
            }
        }
    }
    LOG_TRACE << "userSSL=" << useSSL_ << " domain=" << domain_;
}

void WebSocketClientImpl::sendReq(const trantor::TcpConnectionPtr &connPtr)
{
    trantor::MsgBuffer buffer;
    assert(upgradeRequest_);
    auto implPtr = static_cast<HttpRequestImpl *>(upgradeRequest_.get());
    implPtr->appendToBuffer(&buffer);
    LOG_TRACE << "Send request:"
              << std::string(buffer.peek(), buffer.readableBytes());
    connPtr->send(std::move(buffer));
}

void WebSocketClientImpl::connectToServer(
    const HttpRequestPtr &request,
    const WebSocketRequestCallback &callback)
{
    assert(callback);
    if (loop_->isInLoopThread())
    {
        upgradeRequest_ = request;
        requestCallback_ = callback;
        connectToServerInLoop();
    }
    else
    {
        auto thisPtr = shared_from_this();
        loop_->queueInLoop([request, callback, thisPtr] {
            thisPtr->upgradeRequest_ = request;
            thisPtr->requestCallback_ = callback;
            thisPtr->connectToServerInLoop();
        });
    }
}

WebSocketClientPtr WebSocketClient::newWebSocketClient(const std::string &ip,
                                                       uint16_t port,
                                                       bool useSSL,
                                                       trantor::EventLoop *loop,
                                                       bool useOldTLS)
{
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<WebSocketClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL,
        useOldTLS);
}

WebSocketClientPtr WebSocketClient::newWebSocketClient(
    const std::string &hostString,
    trantor::EventLoop *loop,
    bool useOldTLS)
{
    return std::make_shared<WebSocketClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        useOldTLS);
}
