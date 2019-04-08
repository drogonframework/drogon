/**
 *
 *  WebSocketClientImpl.h
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

#pragma once

#include "WebSocketConnectionImpl.h"
#include <drogon/WebSocketClient.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/TcpClient.h>

#include <string>
#include <memory>

namespace drogon
{

class WebSocketClientImpl : public WebSocketClient, public std::enable_shared_from_this<WebSocketClientImpl>
{
  public:
    virtual WebSocketConnectionPtr getConnection() override
    {
        return _websockConnPtr;
    }

    virtual void setMessageHandler(const std::function<void(std::string &&message,
                                                            const WebSocketClientPtr &,
                                                            const WebSocketMessageType &)> &callback) override
    {
        _messageCallback = callback;
    }

    virtual void setConnectionClosedHandler(const std::function<void(const WebSocketClientPtr &)> &callback) override
    {
        _connectionClosedCallback = callback;
    }

    virtual void setHeartbeatMessage(const std::string &message, double interval) override;

    virtual void connectToServer(const HttpRequestPtr &request, const WebSocketRequestCallback &callback) override
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

    virtual trantor::EventLoop *getLoop() override { return _loop; }

    WebSocketClientImpl(trantor::EventLoop *loop, const trantor::InetAddress &addr, bool useSSL = false);

    WebSocketClientImpl(trantor::EventLoop *loop, const std::string &hostString);

    ~WebSocketClientImpl();

  private:
    std::shared_ptr<trantor::TcpClient> _tcpClient;
    trantor::EventLoop *_loop;
    trantor::InetAddress _server;
    std::string _domain;
    bool _useSSL;
    bool _upgraded = false;
    std::string _wsKey;
    std::string _wsAccept;
    trantor::TimerId _heartbeatTimerId;

    HttpRequestPtr _upgradeRequest;
    std::function<void(std::string &&message, const WebSocketClientPtr &, const WebSocketMessageType &)> _messageCallback = [](std::string &&message, const WebSocketClientPtr &, const WebSocketMessageType &) {};
    std::function<void(const WebSocketClientPtr &)> _connectionClosedCallback = [](const WebSocketClientPtr &) {};
    WebSocketRequestCallback _requestCallback;
    WebSocketConnectionImplPtr _websockConnPtr;

    void connectToServerInLoop();
    void sendReq(const trantor::TcpConnectionPtr &connPtr);
    void onRecvMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onRecvWsMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void reconnect();
};

} // namespace drogon