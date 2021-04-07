/**
 *
 *  @file TcpConnection.h
 *  @author An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#pragma once
#include <trantor/exports.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/net/callbacks.h>
#include <memory>
#include <functional>
#include <string>

namespace trantor
{
class SSLContext;
TRANTOR_EXPORT std::shared_ptr<SSLContext> newSSLServerContext(
    const std::string &certPath,
    const std::string &keyPath,
    bool useOldTLS = false);
/**
 * @brief This class represents a TCP connection.
 *
 */
class TRANTOR_EXPORT TcpConnection
{
  public:
    TcpConnection() = default;
    virtual ~TcpConnection(){};

    /**
     * @brief Send some data to the peer.
     *
     * @param msg
     * @param len
     */
    virtual void send(const char *msg, size_t len) = 0;
    virtual void send(const void *msg, size_t len) = 0;
    virtual void send(const std::string &msg) = 0;
    virtual void send(std::string &&msg) = 0;
    virtual void send(const MsgBuffer &buffer) = 0;
    virtual void send(MsgBuffer &&buffer) = 0;
    virtual void send(const std::shared_ptr<std::string> &msgPtr) = 0;
    virtual void send(const std::shared_ptr<MsgBuffer> &msgPtr) = 0;

    /**
     * @brief Send a file to the peer.
     *
     * @param fileName
     * @param offset
     * @param length
     */
    virtual void sendFile(const char *fileName,
                          size_t offset = 0,
                          size_t length = 0) = 0;

    /**
     * @brief Get the local address of the connection.
     *
     * @return const InetAddress&
     */
    virtual const InetAddress &localAddr() const = 0;

    /**
     * @brief Get the remote address of the connection.
     *
     * @return const InetAddress&
     */
    virtual const InetAddress &peerAddr() const = 0;

    /**
     * @brief Return true if the connection is established.
     *
     * @return true
     * @return false
     */
    virtual bool connected() const = 0;

    /**
     * @brief Return false if the connection is established.
     *
     * @return true
     * @return false
     */
    virtual bool disconnected() const = 0;

    /**
     * @brief Get the buffer in which the received data stored.
     *
     * @return MsgBuffer*
     */
    virtual MsgBuffer *getRecvBuffer() = 0;

    /**
     * @brief Set the high water mark callback
     *
     * @param cb The callback is called when the data in sending buffer is
     * larger than the water mark.
     * @param markLen The water mark in bytes.
     */
    virtual void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                          size_t markLen) = 0;

    /**
     * @brief Set the TCP_NODELAY option to the socket.
     *
     * @param on
     */
    virtual void setTcpNoDelay(bool on) = 0;

    /**
     * @brief Shutdown the connection.
     * @note This method only closes the writing direction.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Close the connection forcefully.
     *
     */
    virtual void forceClose() = 0;

    /**
     * @brief Get the event loop in which the connection I/O is handled.
     *
     * @return EventLoop*
     */
    virtual EventLoop *getLoop() = 0;

    /**
     * @brief Set the custom data on the connection.
     *
     * @param context
     */
    void setContext(const std::shared_ptr<void> &context)
    {
        contextPtr_ = context;
    }
    void setContext(std::shared_ptr<void> &&context)
    {
        contextPtr_ = std::move(context);
    }

    /**
     * @brief Get the custom data from the connection.
     *
     * @tparam T
     * @return std::shared_ptr<T>
     */
    template <typename T>
    std::shared_ptr<T> getContext() const
    {
        return std::static_pointer_cast<T>(contextPtr_);
    }

    /**
     * @brief Return true if the custom data is set by user.
     *
     * @return true
     * @return false
     */
    bool hasContext() const
    {
        return (bool)contextPtr_;
    }

    /**
     * @brief Clear the custom data.
     *
     */
    void clearContext()
    {
        contextPtr_.reset();
    }

    /**
     * @brief Call this method to avoid being kicked off by TcpServer, refer to
     * the kickoffIdleConnections method in the TcpServer class.
     *
     */
    virtual void keepAlive() = 0;

    /**
     * @brief Return true if the keepAlive() method is called.
     *
     * @return true
     * @return false
     */
    virtual bool isKeepAlive() = 0;

    /**
     * @brief Return the number of bytes sent
     *
     * @return size_t
     */
    virtual size_t bytesSent() const = 0;

    /**
     * @brief Return the number of bytes received.
     *
     * @return size_t
     */
    virtual size_t bytesReceived() const = 0;

    /**
     * @brief Check whether the connection is SSL encrypted.
     *
     * @return true
     * @return false
     */
    virtual bool isSSLConnection() const = 0;

    /**
     * @brief Start the SSL encryption on the connection (as a client).
     *
     * @param callback The callback is called when the SSL connection is
     * established.
     * @param hostname The server hostname for SNI. If it is empty, the SNI is
     * not used.
     */
    virtual void startClientEncryption(std::function<void()> callback,
                                       bool useOldTLS = false,
                                       bool validateCert = true,
                                       std::string hostname = "") = 0;

    /**
     * @brief Start the SSL encryption on the connection (as a server).
     *
     * @param ctx The SSL context.
     * @param callback The callback is called when the SSL connection is
     * established.
     */
    virtual void startServerEncryption(const std::shared_ptr<SSLContext> &ctx,
                                       std::function<void()> callback) = 0;

  protected:
    bool validateCert_ = false;

  private:
    std::shared_ptr<void> contextPtr_;
};

}  // namespace trantor
