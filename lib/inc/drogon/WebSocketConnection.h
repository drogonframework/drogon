/**
 *
 *  @file WebSocketConnection.h
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

#include <memory>
#include <string>
#include <drogon/HttpTypes.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
enum class CloseCode
{
    /*1000 indicates a normal closure, meaning that the purpose for which the
       connection was established has been fulfilled.*/
    kNormalClosure = 1000,
    /*1001 indicates that an endpoint is "going away", such as a server going
       down or a browser having navigated away from a page.*/
    kEndpointGone = 1001,
    /*1002 indicates that an endpoint is terminating the connection due to a
       protocol error.*/
    kProtocolError = 1002,
    /*1003 indicates that an endpoint is terminating the connection because it
       has received a type of data it cannot accept (e.g., an endpoint that
       understands only text data MAY send this if it receives a binary
       message).*/
    kInvalidMessage = 1003,
    /*1005 is a reserved value and MUST NOT be set as a status code in a Close
       control frame by an endpoint.  It is designated for use in applications
       expecting a status code to indicate that no status code was actually
       present.*/
    kNone = 1005,
    /*1006 is a reserved value and MUST NOT be set as a status code in a Close
       control frame by an endpoint.  It is designated for use in applications
       expecting a status code to indicate that the connection was closed
       abnormally, e.g., without sending or receiving a Close control frame.
    */
    kAbnormally = 1006,
    /*1007 indicates that an endpoint is terminating the connection because it
       has received data within a message that was not consistent with the type
       of the message (e.g., non-UTF-8 [RFC3629] data within a text message).*/
    kWrongMessageContent = 1007,
    /*1008 indicates that an endpoint is terminating the connection because it
       has received a message that violates its policy.  This is a generic
       status code that can be returned when there is no other more suitable
       status code (e.g., 1003 or 1009) or if there is a need to hide specific
       details about the policy.
    */
    kViolation = 1008,
    /*1009 indicates that an endpoint is terminating the connection because it
       has received a message that is too big for it to process.*/
    kMessageTooBig = 1009,
    /*1010 indicates that an endpoint (client) is terminating the connection
       because it has expected the server to negotiate one or more extension,
       but the server didn't return them in the response message of the
       WebSocket handshake.  The list of extensions that are needed SHOULD
       appear in the /reason/ part of the Close frame. Note that this status
       code is not used by the server, because it can fail the WebSocket
       handshake instead.*/
    kNeedMoreExtensions = 1010,
    /*1011 indicates that a server is terminating the connection because it
       encountered an unexpected condition that prevented it from fulfilling the
       request.*/
    kUnexpectedCondition = 1011,
    /*1015 is a reserved value and MUST NOT be set as a status code in a Close
       control frame by an endpoint.  It is designated for use in applications
       expecting a status code to indicate that the connection was closed due to
       a failure to perform a TLS handshake (e.g., the server certificate can't
       be verified).*/
    kTLSFailed = 1015
};
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
        const WebSocketMessageType type = WebSocketMessageType::Text) = 0;

    /**
     * @brief Send a message to the peer
     *
     * @param msg The message to be sent.
     * @param type The message type.
     */
    virtual void send(
        const std::string &msg,
        const WebSocketMessageType type = WebSocketMessageType::Text) = 0;

    /// Return the local IP address and port number of the connection
    virtual const trantor::InetAddress &localAddr() const = 0;

    /// Return the remote IP address and port number of the connection
    virtual const trantor::InetAddress &peerAddr() const = 0;

    /// Return true if the connection is open
    virtual bool connected() const = 0;

    /// Return true if the connection is closed
    virtual bool disconnected() const = 0;

    /**
     * @brief Shut down the write direction, which means that further send
     * operations are disabled.
     *
     * @param code Please refer to the enum class CloseCode. (RFC6455 7.4.1)
     * @param reason The reason for closing the connection.
     */
    virtual void shutdown(const CloseCode code = CloseCode::kNormalClosure,
                          const std::string &reason = "") = 0;

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

    /**
     * @brief Get the custom data reference from the connection.
     * @note Please make sure that the context is available.
     * @tparam T The type of the data stored in the context.
     * @return T&
     */
    template <typename T>
    T &getContextRef() const
    {
        return *(static_cast<T *>(contextPtr_.get()));
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
     * An empty ping message is sent every 30 seconds by default. The method
     * overrides the default behavior.
     */
    virtual void setPingMessage(
        const std::string &message,
        const std::chrono::duration<double> &interval) = 0;

    /**
     * @brief Disable sending ping messages to the peer.
     */
    virtual void disablePing() = 0;

  private:
    std::shared_ptr<void> contextPtr_;
};
using WebSocketConnectionPtr = std::shared_ptr<WebSocketConnection>;
}  // namespace drogon
