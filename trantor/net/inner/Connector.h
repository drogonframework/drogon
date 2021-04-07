/**
 *
 *  Connector.h
 *  An Tao
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

#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <atomic>
#include <memory>

namespace trantor
{
class Connector : public NonCopyable,
                  public std::enable_shared_from_this<Connector>
{
  public:
    using NewConnectionCallback = std::function<void(int sockfd)>;
    using ConnectionErrorCallback = std::function<void()>;
    Connector(EventLoop *loop, const InetAddress &addr, bool retry = true);
    Connector(EventLoop *loop, InetAddress &&addr, bool retry = true);
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }
    void setNewConnectionCallback(NewConnectionCallback &&cb)
    {
        newConnectionCallback_ = std::move(cb);
    }
    void setErrorCallback(const ConnectionErrorCallback &cb)
    {
        errorCallback_ = cb;
    }
    void setErrorCallback(ConnectionErrorCallback &&cb)
    {
        errorCallback_ = std::move(cb);
    }
    const InetAddress &serverAddress() const
    {
        return serverAddr_;
    }
    void start();
    void restart();
    void stop();

  private:
    NewConnectionCallback newConnectionCallback_;
    ConnectionErrorCallback errorCallback_;
    enum class Status
    {
        Disconnected,
        Connecting,
        Connected
    };
    static constexpr int kMaxRetryDelayMs = 30 * 1000;
    static constexpr int kInitRetryDelayMs = 500;
    std::shared_ptr<Channel> channelPtr_;
    EventLoop *loop_;
    InetAddress serverAddr_;

    std::atomic_bool connect_{false};
    std::atomic<Status> status_{Status::Disconnected};

    int retryInterval_{kInitRetryDelayMs};
    int maxRetryInterval_{kMaxRetryDelayMs};

    bool retry_;

    void startInLoop();
    void connect();
    void connecting(int sockfd);
    int removeAndResetChannel();
    void handleWrite();
    void handleError();
    void retry(int sockfd);
};

}  // namespace trantor
