/**
 *
 *  WebSocketConnection.h
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

#include <drogon/config.h>
#include <memory>
#include <string>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
enum class WebSocketMessageType
{
    Text,
    Binary,
    Ping,
    Pong,
    Close,
    Unknown
};

class WebSocketConnection
{
  public:
    WebSocketConnection() = default;
    virtual ~WebSocketConnection(){};
    virtual void send(const char *msg, uint64_t len, const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;
    virtual void send(const std::string &msg, const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;

    virtual const trantor::InetAddress &localAddr() const = 0;
    virtual const trantor::InetAddress &peerAddr() const = 0;

    virtual bool connected() const = 0;
    virtual bool disconnected() const = 0;

    virtual void shutdown() = 0;    // close write
    virtual void forceClose() = 0;  // close

    virtual void setContext(const any &context) = 0;
    virtual const any &getContext() const = 0;
    virtual any *getMutableContext() = 0;

    /// Set the heartbeat(ping) message sent to the server.
    /**
     * NOTE:
     * Both the server and the client in Drogon automatically send the pong
     * message after receiving the ping message.
     */
    virtual void setPingMessage(const std::string &message, const std::chrono::duration<long double> &interval) = 0;
};
typedef std::shared_ptr<WebSocketConnection> WebSocketConnectionPtr;
}  // namespace drogon
