/**
 *
 *  @file WebSocketConnectionImpl.h
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

#include "impl_forwards.h"
#include <drogon/WebSocketConnection.h>
#include <string_view>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/TcpConnection.h>

namespace drogon
{
class WebSocketConnectionImpl;
using WebSocketConnectionImplPtr = std::shared_ptr<WebSocketConnectionImpl>;

class WebSocketMessageParser
{
  public:
    bool parse(trantor::MsgBuffer *buffer);

    bool gotAll(std::string &message, WebSocketMessageType &type)
    {
        assert(message.empty());
        if (!gotAll_)
            return false;
        message.swap(message_);
        type = type_;
        return true;
    }

  private:
    std::string message_;
    WebSocketMessageType type_;
    bool gotAll_{false};
};

class WebSocketConnectionImpl final
    : public WebSocketConnection,
      public std::enable_shared_from_this<WebSocketConnectionImpl>,
      public trantor::NonCopyable
{
  public:
    explicit WebSocketConnectionImpl(const trantor::TcpConnectionPtr &conn,
                                     bool isServer = true);

    ~WebSocketConnectionImpl() override;
    void send(
        const char *msg,
        uint64_t len,
        const WebSocketMessageType type = WebSocketMessageType::Text) override;
    void send(
        std::string_view msg,
        const WebSocketMessageType type = WebSocketMessageType::Text) override;

    const trantor::InetAddress &localAddr() const override;
    const trantor::InetAddress &peerAddr() const override;

    bool connected() const override;
    bool disconnected() const override;

    void shutdown(const CloseCode code = CloseCode::kNormalClosure,
                  const std::string &reason = "") override;  // close write
    void forceClose() override;                              // close

    void setPingMessage(const std::string &message,
                        const std::chrono::duration<double> &interval) override;

    void disablePing() override;

    void setMessageCallback(
        const std::function<void(std::string &&,
                                 const WebSocketConnectionImplPtr &,
                                 const WebSocketMessageType &)> &callback)
    {
        messageCallback_ = callback;
    }

    void setCloseCallback(
        const std::function<void(const WebSocketConnectionImplPtr &)> &callback)
    {
        closeCallback_ = callback;
    }

    void onNewMessage(const trantor::TcpConnectionPtr &connPtr,
                      trantor::MsgBuffer *buffer);

    void onClose()
    {
        if (pingTimerId_ != trantor::InvalidTimerId)
            tcpConnectionPtr_->getLoop()->invalidateTimer(pingTimerId_);
        closeCallback_(shared_from_this());
    }

  private:
    trantor::TcpConnectionPtr tcpConnectionPtr_;
    trantor::InetAddress localAddr_;
    trantor::InetAddress peerAddr_;
    bool isServer_{true};
    WebSocketMessageParser parser_;
    trantor::TimerId pingTimerId_{trantor::InvalidTimerId};
    std::vector<uint32_t> masks_;
    std::atomic<bool> usingMask_;

    std::function<void(std::string &&,
                       const WebSocketConnectionImplPtr &,
                       const WebSocketMessageType &)>
        messageCallback_ = [](std::string &&,
                              const WebSocketConnectionImplPtr &,
                              const WebSocketMessageType &) {};
    std::function<void(const WebSocketConnectionImplPtr &)> closeCallback_ =
        [](const WebSocketConnectionImplPtr &) {};
    void sendWsData(const char *msg, uint64_t len, unsigned char opcode);
    void disablePingInLoop();
    void setPingMessageInLoop(std::string &&message,
                              const std::chrono::duration<double> &interval);
};

}  // namespace drogon
