/**
 *
 *  WebSocketClientImpl.cc
 *  An Tao
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
    return _websockConnPtr;
}
void WebSocketClientImpl::createTcpClient()
{
    LOG_TRACE << "New TcpClient," << _server.toIpPort();
    _tcpClient =
        std::make_shared<trantor::TcpClient>(_loop, _server, "httpClient");
    if (_useSSL)
    {
        _tcpClient->enableSSL();
    }
    auto thisPtr = shared_from_this();
    std::weak_ptr<WebSocketClientImpl> weakPtr = thisPtr;

    _tcpClient->setConnectionCallback(
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
                thisPtr->_connectionClosedCallback(thisPtr);
                thisPtr->_websockConnPtr.reset();
                thisPtr->_loop->runAfter(1.0,
                                         [thisPtr]() { thisPtr->reconnect(); });
            }
        });
    _tcpClient->setConnectionErrorCallback([weakPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        // can't connect to server
        thisPtr->_requestCallback(ReqResult::NetworkFailure, nullptr, thisPtr);
        thisPtr->_loop->runAfter(1.0, [thisPtr]() { thisPtr->reconnect(); });
    });
    _tcpClient->setMessageCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr,
                  trantor::MsgBuffer *msg) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                thisPtr->onRecvMessage(connPtr, msg);
            }
        });
    _tcpClient->connect();
}
void WebSocketClientImpl::connectToServerInLoop()
{
    _loop->assertInLoopThread();
    _upgradeRequest->addHeader("Connection", "Upgrade");
    _upgradeRequest->addHeader("Upgrade", "websocket");
    auto randStr = utils::genRandomString(16);
    _wsKey = utils::base64Encode((const unsigned char *)randStr.data(),
                                 (unsigned int)randStr.length());

    auto wsKey = _wsKey;
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()),
         wsKey.length(),
         accKey);
    _wsAccept = utils::base64Encode(accKey, SHA_DIGEST_LENGTH);

    _upgradeRequest->addHeader("Sec-WebSocket-Key", _wsKey);
    //_upgradeRequest->addHeader("Sec-WebSocket-Version","13");

    assert(!_tcpClient);

    bool hasIpv6Address = false;
    if (_server.isIpV6())
    {
        auto ipaddr = _server.ip6NetEndian();
        for (int i = 0; i < 4; i++)
        {
            if (ipaddr[i] != 0)
            {
                hasIpv6Address = true;
                break;
            }
        }
    }

    if (_server.ipNetEndian() == 0 && !hasIpv6Address && !_domain.empty() &&
        _server.portNetEndian() != 0)
    {
        if (!_resolver)
        {
            _resolver = trantor::Resolver::newResolver(_loop);
        }
        _resolver->resolve(
            _domain,
            [thisPtr = shared_from_this(),
             hasIpv6Address](const trantor::InetAddress &addr) {
                thisPtr->_loop->runInLoop([thisPtr, addr, hasIpv6Address]() {
                    auto port = thisPtr->_server.portNetEndian();
                    thisPtr->_server = addr;
                    thisPtr->_server.setPortNetEndian(port);
                    LOG_TRACE << "dns:domain=" << thisPtr->_domain
                              << ";ip=" << thisPtr->_server.toIp();
                    if ((thisPtr->_server.ipNetEndian() != 0 ||
                         hasIpv6Address) &&
                        thisPtr->_server.portNetEndian() != 0)
                    {
                        thisPtr->createTcpClient();
                    }
                    else
                    {
                        thisPtr->_requestCallback(ReqResult::BadServerAddress,
                                                  nullptr,
                                                  thisPtr);
                        return;
                    }
                });
            });
        return;
    }

    if ((_server.ipNetEndian() != 0 || hasIpv6Address) &&
        _server.portNetEndian() != 0)
    {
        createTcpClient();
    }
    else
    {
        _requestCallback(ReqResult::BadServerAddress,
                         nullptr,
                         shared_from_this());
        return;
    }
}

void WebSocketClientImpl::onRecvWsMessage(
    const trantor::TcpConnectionPtr &connPtr,
    trantor::MsgBuffer *msgBuffer)
{
    assert(_websockConnPtr);
    _websockConnPtr->onNewMessage(connPtr, msgBuffer);
}

void WebSocketClientImpl::onRecvMessage(
    const trantor::TcpConnectionPtr &connPtr,
    trantor::MsgBuffer *msgBuffer)
{
    if (_upgraded)
    {
        onRecvWsMessage(connPtr, msgBuffer);
        return;
    }
    auto responseParser = connPtr->getContext<HttpResponseParser>();

    // LOG_TRACE << "###:" << msg->readableBytes();

    if (!responseParser->parseResponse(msgBuffer))
    {
        _requestCallback(ReqResult::BadResponse, nullptr, shared_from_this());
        connPtr->shutdown();
        _websockConnPtr.reset();
        _tcpClient.reset();
        return;
    }

    if (responseParser->gotAll())
    {
        auto resp = responseParser->responseImpl();
        responseParser->reset();
        auto acceptStr = resp->getHeaderBy("sec-websocket-accept");

        if (resp->statusCode() != k101SwitchingProtocols ||
            acceptStr != _wsAccept)
        {
            _requestCallback(ReqResult::BadResponse,
                             nullptr,
                             shared_from_this());
            connPtr->shutdown();
            _websockConnPtr.reset();
            _tcpClient.reset();
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

        _upgraded = true;
        _websockConnPtr =
            std::make_shared<WebSocketConnectionImpl>(connPtr, false);
        auto thisPtr = shared_from_this();
        _websockConnPtr->setMessageCallback(
            [thisPtr](std::string &&message,
                      const WebSocketConnectionImplPtr &,
                      const WebSocketMessageType &type) {
                thisPtr->_messageCallback(std::move(message), thisPtr, type);
            });
        _requestCallback(ReqResult::Ok, resp, shared_from_this());
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
    _tcpClient.reset();
    _websockConnPtr.reset();
    _upgraded = false;
    connectToServerInLoop();
}

WebSocketClientImpl::WebSocketClientImpl(trantor::EventLoop *loop,
                                         const trantor::InetAddress &addr,
                                         bool useSSL)
    : _loop(loop), _server(addr), _useSSL(useSSL)
{
}

WebSocketClientImpl::WebSocketClientImpl(trantor::EventLoop *loop,
                                         const std::string &hostString)
    : _loop(loop)
{
    auto lowerHost = hostString;
    std::transform(lowerHost.begin(),
                   lowerHost.end(),
                   lowerHost.begin(),
                   tolower);
    if (lowerHost.find("wss://") != std::string::npos)
    {
        _useSSL = true;
        lowerHost = lowerHost.substr(6);
    }
    else if (lowerHost.find("ws://") != std::string::npos)
    {
        _useSSL = false;
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
        _domain = lowerHost.substr(1, pos - 1);
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
                _server = InetAddress(_domain, port, true);
            }
        }
        else
        {
            if (_useSSL)
            {
                _server = InetAddress(_domain, 443, true);
            }
            else
            {
                _server = InetAddress(_domain, 80, true);
            }
        }
    }
    else
    {
        auto pos = lowerHost.find(':');
        if (pos != std::string::npos)
        {
            _domain = lowerHost.substr(0, pos);
            auto portStr = lowerHost.substr(pos + 1);
            pos = portStr.find('/');
            if (pos != std::string::npos)
            {
                portStr = portStr.substr(0, pos);
            }
            auto port = atoi(portStr.c_str());
            if (port > 0 && port < 65536)
            {
                _server = InetAddress(_domain, port);
            }
        }
        else
        {
            _domain = lowerHost;
            pos = _domain.find('/');
            if (pos != std::string::npos)
            {
                _domain = _domain.substr(0, pos);
            }
            if (_useSSL)
            {
                _server = InetAddress(_domain, 443);
            }
            else
            {
                _server = InetAddress(_domain, 80);
            }
        }
    }
    LOG_TRACE << "userSSL=" << _useSSL << " domain=" << _domain;
}

void WebSocketClientImpl::sendReq(const trantor::TcpConnectionPtr &connPtr)
{
    trantor::MsgBuffer buffer;
    assert(_upgradeRequest);
    auto implPtr = static_cast<HttpRequestImpl *>(_upgradeRequest.get());
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
    if (_loop->isInLoopThread())
    {
        _upgradeRequest = request;
        _requestCallback = callback;
        connectToServerInLoop();
    }
    else
    {
        auto thisPtr = shared_from_this();
        _loop->queueInLoop([request, callback, thisPtr] {
            thisPtr->_upgradeRequest = request;
            thisPtr->_requestCallback = callback;
            thisPtr->connectToServerInLoop();
        });
    }
}

WebSocketClientPtr WebSocketClient::newWebSocketClient(const std::string &ip,
                                                       uint16_t port,
                                                       bool useSSL,
                                                       trantor::EventLoop *loop)
{
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<WebSocketClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL);
}

WebSocketClientPtr WebSocketClient::newWebSocketClient(
    const std::string &hostString,
    trantor::EventLoop *loop)
{
    return std::make_shared<WebSocketClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString);
}
