/**
 *
 *  WebSocketClient.h
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

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/WebSocketConnection.h>
#include <functional>
#include <memory>
#include <string>
#include <trantor/net/EventLoop.h>

namespace drogon
{
class WebSocketClient;
typedef std::shared_ptr<WebSocketClient> WebSocketClientPtr;
typedef std::function<void(ReqResult, const HttpResponsePtr &, const WebSocketClientPtr &)> WebSocketRequestCallback;

/// WebSocket client abstract class
class WebSocketClient
{
  public:
    /// Get the WebSocket connection that is typically used to send messages.
    virtual WebSocketConnectionPtr getConnection() = 0;

    /// Set messages handler. When a message is recieved from the server, the
    /// @param callback is called.
    virtual void setMessageHandler(
        const std::function<void(std::string &&message, const WebSocketClientPtr &, const WebSocketMessageType &)>
            &callback) = 0;

    /// Set the connection handler. When the connection is established or closed,
    /// the @param callback is called with a bool
    /// parameter.
    virtual void setConnectionClosedHandler(const std::function<void(const WebSocketClientPtr &)> &callback) = 0;

    /// Connect to the server.
    virtual void connectToServer(const HttpRequestPtr &request, const WebSocketRequestCallback &callback) = 0;

    /// Get the event loop of the client;
    virtual trantor::EventLoop *getLoop() = 0;

    /// Use ip and port to connect to server
    /**
     * If useSSL is set to true, the client
     * connects to the server using SSL.
     *
     * If the loop parameter is set to nullptr, the client
     * uses the HttpAppFramework's event loop, otherwise it
     * runs in the loop identified by the parameter.
     *
     * Note: The @param ip support for both ipv4 and ipv6 address
     */
    static WebSocketClientPtr newWebSocketClient(const std::string &ip,
                                                 uint16_t port,
                                                 bool useSSL = false,
                                                 trantor::EventLoop *loop = nullptr);

    /// Use hostString to connect to server
    /**
     * Examples for hostString:
     * wss://www.google.com
     * ws://www.google.com
     * wss://127.0.0.1:8080/
     * ws://127.0.0.1
     *
     * The @param hostString must be prefixed by 'ws://' or 'wss://'
     * and doesn't support for ipv6 address if the host is in ip format
     *
     * If the @param loop is set to nullptr, the client
     * uses the HttpAppFramework's main event loop, otherwise it
     * runs in the loop identified by the parameter.
     *
     * NOTE:
     * Don't add path and parameters in hostString, the request path
     * and parameters should be set in HttpRequestPtr when calling
     * the connectToServer() method.
     *
     */
    static WebSocketClientPtr newWebSocketClient(const std::string &hostString, trantor::EventLoop *loop = nullptr);

    virtual ~WebSocketClient()
    {
    }
};

}  // namespace drogon