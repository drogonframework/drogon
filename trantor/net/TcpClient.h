// taken from muduo
// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

// Copyright 2016, Tao An.  All rights reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Tao An

#pragma once
#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/exports.h>
#include <functional>
#include <thread>
#include <atomic>
#include <signal.h>
namespace trantor
{
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;
class SSLContext;
/**
 * @brief This class represents a TCP client.
 *
 */
class TRANTOR_EXPORT TcpClient : NonCopyable
{
  public:
    /**
     * @brief Construct a new TCP client instance.
     *
     * @param loop The event loop in which the client runs.
     * @param serverAddr The address of the server.
     * @param nameArg The name of the client.
     */
    TcpClient(EventLoop *loop,
              const InetAddress &serverAddr,
              const std::string &nameArg);
    ~TcpClient();

    /**
     * @brief Connect to the server.
     *
     */
    void connect();

    /**
     * @brief Disconnect from the server.
     *
     */
    void disconnect();

    /**
     * @brief Stop connecting to the server.
     *
     */
    void stop();

    /**
     * @brief Get the TCP connection to the server.
     *
     * @return TcpConnectionPtr
     */
    TcpConnectionPtr connection() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    /**
     * @brief Get the event loop.
     *
     * @return EventLoop*
     */
    EventLoop *getLoop() const
    {
        return loop_;
    }

    /**
     * @brief Check whether the client re-connect to the server.
     *
     * @return true
     * @return false
     */
    bool retry() const
    {
        return retry_;
    }

    /**
     * @brief Enable retrying.
     *
     */
    void enableRetry()
    {
        retry_ = true;
    }

    /**
     * @brief Get the name of the client.
     *
     * @return const std::string&
     */
    const std::string &name() const
    {
        return name_;
    }

    /**
     * @brief Set the connection callback.
     *
     * @param cb The callback is called when the connection to the server is
     * established or closed.
     */
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }
    void setConnectionCallback(ConnectionCallback &&cb)
    {
        connectionCallback_ = std::move(cb);
    }

    /**
     * @brief Set the connection error callback.
     *
     * @param cb The callback is called when an error occurs during connecting
     * to the server.
     */
    void setConnectionErrorCallback(const ConnectionErrorCallback &cb)
    {
        connectionErrorCallback_ = cb;
    }

    /**
     * @brief Set the message callback.
     *
     * @param cb The callback is called when some data is received from the
     * server.
     */
    void setMessageCallback(const RecvMessageCallback &cb)
    {
        messageCallback_ = cb;
    }
    void setMessageCallback(RecvMessageCallback &&cb)
    {
        messageCallback_ = std::move(cb);
    }
    /// Set write complete callback.
    /// Not thread safe.

    /**
     * @brief Set the write complete callback.
     *
     * @param cb The callback is called when data to send is written to the
     * socket.
     */
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }
    void setWriteCompleteCallback(WriteCompleteCallback &&cb)
    {
        writeCompleteCallback_ = std::move(cb);
    }

    /**
     * @brief Set the callback for errors of SSL
     * @param cb The callback is called when an SSL error occurs.
     */
    void setSSLErrorCallback(const SSLErrorCallback &cb)
    {
        sslErrorCallback_ = cb;
    }
    void setSSLErrorCallback(SSLErrorCallback &&cb)
    {
        sslErrorCallback_ = std::move(cb);
    }

    /**
     * @brief Enable SSL encryption.
     * @param useOldTLS If true, the TLS 1.0 and 1.1 are supported by the
     * client.
     * @param hostname The server hostname for SNI. If it is empty, the SNI is
     * not used.
     * @param validateCert If true, we try to validate if the peer's SSL cert
     * is valid.
     * @note It's well known that TLS 1.0 and 1.1 are not considered secure in
     * 2020. And it's a good practice to only use TLS 1.2 and above.
     */
    void enableSSL(bool useOldTLS = false,
                   bool validateCert = true,
                   std::string hostname = "");

  private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd);
    /// Not thread safe, but in loop
    void removeConnection(const TcpConnectionPtr &conn);

    EventLoop *loop_;
    ConnectorPtr connector_;  // avoid revealing Connector
    const std::string name_;
    ConnectionCallback connectionCallback_;
    ConnectionErrorCallback connectionErrorCallback_;
    RecvMessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    SSLErrorCallback sslErrorCallback_;
    std::atomic_bool retry_;    // atomic
    std::atomic_bool connect_;  // atomic
    // always in loop thread
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;  // @GuardedBy mutex_
    std::shared_ptr<SSLContext> sslCtxPtr_;
    bool validateCert_{false};
    std::string SSLHostName_;
#ifndef _WIN32
    class IgnoreSigPipe
    {
      public:
        IgnoreSigPipe()
        {
            ::signal(SIGPIPE, SIG_IGN);
        }
    };

    static IgnoreSigPipe initObj;
#endif
};

}  // namespace trantor
