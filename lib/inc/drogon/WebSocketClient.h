/**
 *
 *  @file WebSocketClient.h
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

#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/HttpTypes.h>
#include <functional>
#include <memory>
#include <string>
#include <trantor/net/EventLoop.h>

namespace drogon
{
class WebSocketClient;
using WebSocketClientPtr = std::shared_ptr<WebSocketClient>;
using WebSocketRequestCallback = std::function<
    void(ReqResult, const HttpResponsePtr &, const WebSocketClientPtr &)>;

/**
 * @brief WebSocket client abstract class
 *
 */
class WebSocketClient
{
  public:
    /// Get the WebSocket connection that is typically used to send messages.
    virtual WebSocketConnectionPtr getConnection() = 0;

    /**
     * @brief Set messages handler. When a message is recieved from the server,
     * the callback is called.
     *
     * @param callback The function to call when a message is received.
     */
    virtual void setMessageHandler(
        const std::function<void(std::string &&message,
                                 const WebSocketClientPtr &,
                                 const WebSocketMessageType &)> &callback) = 0;

    /// Set the connection closing handler. When the connection is established
    /// or closed, the @param callback is called with a bool parameter.

    /**
     * @brief Set the connection closing handler. When the websocket connection
     * is closed, the  callback is called
     *
     * @param callback The function to call when the connection is closed.
     */
    virtual void setConnectionClosedHandler(
        const std::function<void(const WebSocketClientPtr &)> &callback) = 0;

    /// Connect to the server.
    virtual void connectToServer(const HttpRequestPtr &request,
                                 const WebSocketRequestCallback &callback) = 0;

    /// Get the event loop of the client;
    virtual trantor::EventLoop *getLoop() = 0;

    /**
     * @brief Create a websocket client using the given ip and port to connect
     * to server.
     *
     * @param ip The ip address of the server.
     * @param port The port of the server.
     * @param useSSL If useSSL is set to true, the client connects to the server
     * using SSL.
     * @param loop If the loop parameter is set to nullptr, the client uses the
     * HttpAppFramework's event loop, otherwise it runs in the loop identified
     * by the parameter.
     * @param useOldTLS If the parameter is set to true, the TLS1.0/1.1 are
     * enabled for HTTPS.
     * @return WebSocketClientPtr The smart pointer to the WebSocket client.
     * @note The ip parameter support for both ipv4 and ipv6 address
     */
    static WebSocketClientPtr newWebSocketClient(
        const std::string &ip,
        uint16_t port,
        bool useSSL = false,
        trantor::EventLoop *loop = nullptr,
        bool useOldTLS = false);

    /// Create a websocket client using the given hostString to connect to
    /// server
    /**
     * @param hostString must be prefixed by 'ws://' or 'wss://'
     * Examples for hostString:
     * @code
       wss://www.google.com
       ws://www.google.com
       wss://127.0.0.1:8080/
       ws://127.0.0.1
       @endcode
     * @param loop if the parameter is set to nullptr, the client uses the
     * HttpAppFramework's main event loop, otherwise it runs in the loop
     * identified by the parameter.
     * @param useOldTLS If the parameter is set to true, the TLS1.0/1.1 are
     * enabled for HTTPS.
     * @note
     * Don't add path and parameters in hostString, the request path and
     * parameters should be set in HttpRequestPtr when calling the
     * connectToServer() method.
     *
     */
    static WebSocketClientPtr newWebSocketClient(
        const std::string &hostString,
        trantor::EventLoop *loop = nullptr,
        bool useOldTLS = false);

    virtual ~WebSocketClient()
    {
    }
};

}  // namespace drogon
