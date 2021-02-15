/**
 *
 *  @file RedisConnection.h
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
#include <drogon/utils/string_view.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <hiredis/async.h>
#include <memory>

namespace drogon
{
namespace nosql
{
class RedisResult;
class RedisConnection : public trantor::NonCopyable,
                        public std::enable_shared_from_this<RedisConnection>
{
  public:
    RedisConnection(const trantor::InetAddress &serverAddress,
                    const std::string &password,
                    trantor::EventLoop *loop);
    void setConnectCallback(const std::function<void(int)> &callback)
    {
        connectCallback_ = callback;
    }
    void setDisconnectCallback(const std::function<void(int)> &callback)
    {
        disconnectCallback_ = callback;
    }
    void sendCommand(
        const std::string &command,
        std::function<void(const RedisResult &)> &&callback,
        std::function<void(const std::exception &)> &&exceptionCallback,
        ...)
    {
        if (loop_->isInLoopThread())
        {
            sendCommandInloop(command,
                              std::move(callback),
                              std::move(exceptionCallback));
        }
        else
        {
            loop_->queueInLoop(
                [this,
                 callback = std::move(callback),
                 exceptionCallback = std::move(exceptionCallback),
                 command]() mutable {
                    sendCommandInloop(command,
                                      std::move(callback),
                                      std::move(exceptionCallback));
                });
        }
    }

  private:
    redisAsyncContext *redisContext_{nullptr};
    const trantor::InetAddress serverAddr_;
    const std::string password_;
    trantor::EventLoop *loop_{nullptr};
    std::unique_ptr<trantor::Channel> channel_{nullptr};
    std::function<void(int)> connectCallback_;
    std::function<void(int)> disconnectCallback_;
    std::function<void(const RedisResult &)> commandCallback_;
    std::function<void(const std::exception &)> exceptionCallback_;
    string_view command_;
    bool connected_{false};
    void startConnectionInLoop();
    static void addWrite(void *userData);
    static void delWrite(void *userData);
    static void addRead(void *userData);
    static void delRead(void *userData);
    static void cleanup(void *userData);
    void handleRedisRead();
    void handleRedisWrite();
    void handleResult(redisReply *result);
    void sendCommandInloop(
        const std::string &command,
        std::function<void(const RedisResult &)> &&callback,
        std::function<void(const std::exception &)> &&exceptionCallback,
        ...);
};
}  // namespace nosql
}  // namespace drogon