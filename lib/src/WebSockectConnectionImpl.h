/**
 *
 *  WebSocketConnectionImpl.h
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

#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>
namespace drogon
{
class WebSocketConnectionImpl : public WebSocketConnection
{
  public:
    explicit WebSocketConnectionImpl(const trantor::TcpConnectionPtr &conn, bool isServer = true);

    virtual void send(const char *msg, uint64_t len, const WebSocketMessageType &type = WebSocketMessageType::Text) override;
    virtual void send(const std::string &msg, const WebSocketMessageType &type = WebSocketMessageType::Text) override;

    virtual const trantor::InetAddress &localAddr() const override;
    virtual const trantor::InetAddress &peerAddr() const override;

    virtual bool connected() const override;
    virtual bool disconnected() const override;

    virtual void shutdown() override;   //close write
    virtual void forceClose() override; //close

    virtual void setContext(const any &context) override;
    virtual const any &getContext() const override;
    virtual any *getMutableContext() override;

    void setController(const WebSocketControllerBasePtr &ctrl)
    {
        _ctrlPtr = ctrl;
    }
    WebSocketControllerBasePtr controller()
    {
        return _ctrlPtr;
    }

  private:
    std::weak_ptr<trantor::TcpConnection> _tcpConn;
    trantor::InetAddress _localAddr;
    trantor::InetAddress _peerAddr;
    WebSocketControllerBasePtr _ctrlPtr;
    any _context;
    bool _isServer = true;

    void sendWsData(const char *msg, size_t len, unsigned char opcode);
};

typedef std::shared_ptr<WebSocketConnectionImpl> WebSocketConnectionImplPtr;
} // namespace drogon
