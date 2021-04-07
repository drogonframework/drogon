/**
 *
 *  @file TcpConnectionImpl.h
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

#include <trantor/net/TcpConnection.h>
#include <trantor/utils/TimingWheel.h>
#include <list>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <thread>
#include <array>

namespace trantor
{
#ifdef USE_OPENSSL
enum class SSLStatus
{
    Handshaking,
    Connecting,
    Connected,
    DisConnecting,
    DisConnected
};
class SSLContext;
class SSLConn;

std::shared_ptr<SSLContext> newSSLContext(bool useOldTLS, bool validateCert);
std::shared_ptr<SSLContext> newSSLServerContext(const std::string &certPath,
                                                const std::string &keyPath,
                                                bool useOldTLS);
// void initServerSSLContext(const std::shared_ptr<SSLContext> &ctx,
//                           const std::string &certPath,
//                           const std::string &keyPath);
#endif
class Channel;
class Socket;
class TcpServer;
void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn);
class TcpConnectionImpl : public TcpConnection,
                          public NonCopyable,
                          public std::enable_shared_from_this<TcpConnectionImpl>
{
    friend class TcpServer;
    friend class TcpClient;
    friend void trantor::removeConnection(EventLoop *loop,
                                          const TcpConnectionPtr &conn);

  public:
    class KickoffEntry
    {
      public:
        explicit KickoffEntry(const std::weak_ptr<TcpConnection> &conn)
            : conn_(conn)
        {
        }
        void reset()
        {
            conn_.reset();
        }
        ~KickoffEntry()
        {
            auto conn = conn_.lock();
            if (conn)
            {
                conn->forceClose();
            }
        }

      private:
        std::weak_ptr<TcpConnection> conn_;
    };

    TcpConnectionImpl(EventLoop *loop,
                      int socketfd,
                      const InetAddress &localAddr,
                      const InetAddress &peerAddr);
#ifdef USE_OPENSSL
    TcpConnectionImpl(EventLoop *loop,
                      int socketfd,
                      const InetAddress &localAddr,
                      const InetAddress &peerAddr,
                      const std::shared_ptr<SSLContext> &ctxPtr,
                      bool isServer = true,
                      bool validateCert = true,
                      const std::string &hostname = "");
#endif
    virtual ~TcpConnectionImpl();
    virtual void send(const char *msg, size_t len) override;
    virtual void send(const void *msg, size_t len) override;
    virtual void send(const std::string &msg) override;
    virtual void send(std::string &&msg) override;
    virtual void send(const MsgBuffer &buffer) override;
    virtual void send(MsgBuffer &&buffer) override;
    virtual void send(const std::shared_ptr<std::string> &msgPtr) override;
    virtual void send(const std::shared_ptr<MsgBuffer> &msgPtr) override;
    virtual void sendFile(const char *fileName,
                          size_t offset = 0,
                          size_t length = 0) override;

    virtual const InetAddress &localAddr() const override
    {
        return localAddr_;
    }
    virtual const InetAddress &peerAddr() const override
    {
        return peerAddr_;
    }

    virtual bool connected() const override
    {
        return status_ == ConnStatus::Connected;
    }
    virtual bool disconnected() const override
    {
        return status_ == ConnStatus::Disconnected;
    }

    // virtual MsgBuffer* getSendBuffer() override{ return  &writeBuffer_;}
    virtual MsgBuffer *getRecvBuffer() override
    {
        return &readBuffer_;
    }
    // set callbacks
    virtual void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                          size_t markLen) override
    {
        highWaterMarkCallback_ = cb;
        highWaterMarkLen_ = markLen;
    }

    virtual void keepAlive() override
    {
        idleTimeout_ = 0;
        auto entry = kickoffEntry_.lock();
        if (entry)
        {
            entry->reset();
        }
    }
    virtual bool isKeepAlive() override
    {
        return idleTimeout_ == 0;
    }
    virtual void setTcpNoDelay(bool on) override;
    virtual void shutdown() override;
    virtual void forceClose() override;
    virtual EventLoop *getLoop() override
    {
        return loop_;
    }

    virtual size_t bytesSent() const override
    {
        return bytesSent_;
    }
    virtual size_t bytesReceived() const override
    {
        return bytesReceived_;
    }
    virtual void startClientEncryption(std::function<void()> callback,
                                       bool useOldTLS = false,
                                       bool validateCert = true,
                                       std::string hostname = "") override;
    virtual void startServerEncryption(const std::shared_ptr<SSLContext> &ctx,
                                       std::function<void()> callback) override;
    virtual bool isSSLConnection() const override
    {
        return isEncrypted_;
    }

  private:
    /// Internal use only.

    std::weak_ptr<KickoffEntry> kickoffEntry_;
    std::weak_ptr<TimingWheel> timingWheelWeakPtr_;
    size_t idleTimeout_{0};
    Date lastTimingWheelUpdateTime_;

    void enableKickingOff(size_t timeout,
                          const std::shared_ptr<TimingWheel> &timingWheel)
    {
        assert(timingWheel);
        assert(timingWheel->getLoop() == loop_);
        assert(timeout > 0);
        auto entry = std::make_shared<KickoffEntry>(shared_from_this());
        kickoffEntry_ = entry;
        timingWheelWeakPtr_ = timingWheel;
        idleTimeout_ = timeout;
        timingWheel->insertEntry(timeout, entry);
    }
    void extendLife();
#ifndef _WIN32
    void sendFile(int sfd, size_t offset = 0, size_t length = 0);
#else
    void sendFile(FILE *fp, size_t offset = 0, size_t length = 0);
#endif
    void setRecvMsgCallback(const RecvMessageCallback &cb)
    {
        recvMsgCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }
    void setSSLErrorCallback(const SSLErrorCallback &cb)
    {
        sslErrorCallback_ = cb;
    }

    void connectDestroyed();
    virtual void connectEstablished();

  protected:
    struct BufferNode
    {
#ifndef _WIN32
        int sendFd_{-1};
        off_t offset_;
#else
        FILE *sendFp_{nullptr};
        long long offset_;
#endif
        ssize_t fileBytesToSend_;
        std::shared_ptr<MsgBuffer> msgBuffer_;
        ~BufferNode()
        {
#ifndef _WIN32
            if (sendFd_ >= 0)
                close(sendFd_);
#else
            if (sendFp_)
                fclose(sendFp_);
#endif
        }
    };
    using BufferNodePtr = std::shared_ptr<BufferNode>;
    enum class ConnStatus
    {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting
    };
    bool isEncrypted_{false};
    EventLoop *loop_;
    std::unique_ptr<Channel> ioChannelPtr_;
    std::unique_ptr<Socket> socketPtr_;
    MsgBuffer readBuffer_;
    std::list<BufferNodePtr> writeBufferList_;
    void readCallback();
    void writeCallback();
    InetAddress localAddr_, peerAddr_;
    ConnStatus status_{ConnStatus::Connecting};
    // callbacks
    RecvMessageCallback recvMsgCallback_;
    ConnectionCallback connectionCallback_;
    CloseCallback closeCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    SSLErrorCallback sslErrorCallback_;
    void handleClose();
    void handleError();
    // virtual void sendInLoop(const std::string &msg);

    void sendFileInLoop(const BufferNodePtr &file);
#ifndef _WIN32
    void sendInLoop(const void *buffer, size_t length);
    ssize_t writeInLoop(const void *buffer, size_t length);
#else
    void sendInLoop(const char *buffer, size_t length);
    ssize_t writeInLoop(const char *buffer, size_t length);
#endif
    size_t highWaterMarkLen_;
    std::string name_;

    uint64_t sendNum_{0};
    std::mutex sendNumMutex_;

    size_t bytesSent_{0};
    size_t bytesReceived_{0};

    std::unique_ptr<std::vector<char>> fileBufferPtr_;

#ifdef USE_OPENSSL
  private:
    void doHandshaking();
    bool validatePeerCertificate();
    struct SSLEncryption
    {
        SSLStatus statusOfSSL_ = SSLStatus::Handshaking;
        // OpenSSL
        std::shared_ptr<SSLContext> sslCtxPtr_;
        std::unique_ptr<SSLConn> sslPtr_;
        std::unique_ptr<std::array<char, 8192>> sendBufferPtr_;
        bool isServer_{false};
        bool isUpgrade_{false};
        std::function<void()> upgradeCallback_;
        std::string hostname_;
    };
    std::unique_ptr<SSLEncryption> sslEncryptionPtr_;
    void startClientEncryptionInLoop(std::function<void()> &&callback,
                                     bool useOldTLS,
                                     bool validateCert,
                                     const std::string &hostname);
    void startServerEncryptionInLoop(const std::shared_ptr<SSLContext> &ctx,
                                     std::function<void()> &&callback);
#endif
};

using TcpConnectionImplPtr = std::shared_ptr<TcpConnectionImpl>;

}  // namespace trantor
