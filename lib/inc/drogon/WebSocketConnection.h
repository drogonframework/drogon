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

#include <memory>
#include <string>
#include <drogon/HttpTypes.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
/**
 * @brief The WebSocket connection abstract class.
 *
 */
class WebSocketConnection
{
  public:
    WebSocketConnection() = default;
    virtual ~WebSocketConnection(){};

    /**
     * @brief Send a message to the peer
     *
     * @param msg The message to be sent.
     * @param len The message length.
     * @param type The message type.
     */
    virtual void send(
        const char *msg,
        uint64_t len,
        const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;

    /**
     * @brief Send a message to the peer
     *
     * @param msg The message to be sent.
     * @param type The message type.
     */
    virtual void send(
        const std::string &msg,
        const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;

    /// Return the local IP address and port number of the connection
    virtual const trantor::InetAddress &localAddr() const = 0;

    /// Return the remote IP address and port number of the connection
    virtual const trantor::InetAddress &peerAddr() const = 0;

    /// Return true if the connection is open
    virtual bool connected() const = 0;

    /// Return true if the connection is closed
    virtual bool disconnected() const = 0;

    /// Shut down the write direction, which means that further send operations
    /// are disabled.
    virtual void shutdown() = 0;

    /// Close the connection
    virtual void forceClose() = 0;

    /**
     * @brief Set custom data on the connection
     *
     * @param context The custom data.
     */
    void setContext(const std::shared_ptr<void> &context)
    {
        contextPtr_ = context;
    }

    /**
     * @brief Set custom data on the connection
     *
     * @param context The custom data.
     */
    void setContext(std::shared_ptr<void> &&context)
    {
        contextPtr_ = std::move(context);
    }

    /**
     * @brief Get custom data from the connection
     *
     * @tparam T The type of the data
     * @return std::shared_ptr<T> The smart pointer to the data object.
     */
    template <typename T>
    std::shared_ptr<T> getContext() const
    {
        return std::static_pointer_cast<T>(contextPtr_);
    }

    /// Return true if the context is set by user.
    bool hasContext()
    {
        return (bool)contextPtr_;
    }

    /// Clear the context.
    void clearContext()
    {
        contextPtr_.reset();
    }

    /**
     * @brief Set the heartbeat(ping) message sent to the peer.
     *
     * @param message The ping message.
     * @param interval The sending interval.
     * @note
     * Both the server and the client in Drogon automatically send the pong
     * message after receiving the ping message.
     */
    virtual void setPingMessage(
        const std::string &message,
        const std::chrono::duration<long double> &interval) = 0;

  private:
    std::shared_ptr<void> contextPtr_;
};
using WebSocketConnectionPtr = std::shared_ptr<WebSocketConnection>;
}  // namespace drogon
