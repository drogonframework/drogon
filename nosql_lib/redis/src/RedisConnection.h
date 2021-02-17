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
#include <hiredis/hiredis.h>
#include <memory>
#include <queue>

namespace drogon
{
namespace nosql
{
class RedisResult;
enum class ConnectStatus
{
    kNone = 0,
    kConnecting,
    kConnected,
    kEnd
};
class RedisConnection : public trantor::NonCopyable,
                        public std::enable_shared_from_this<RedisConnection>
{
  public:
    RedisConnection(const trantor::InetAddress &serverAddress,
                    const std::string &password,
                    trantor::EventLoop *loop);
    void setConnectCallback(
        const std::function<void(std::shared_ptr<RedisConnection> &&)>
            &callback)
    {
        connectCallback_ = callback;
    }
    void setDisconnectCallback(
        const std::function<void(std::shared_ptr<RedisConnection> &&)>
            &callback)
    {
        disconnectCallback_ = callback;
    }
    void sendCommand(
        const std::string &command,
        std::function<void(const RedisResult &)> &&callback,
        std::function<void(const std::exception &)> &&exceptionCallback,
        ...)
    {
        va_list args;
        va_start(args, exceptionCallback);
        sendvCommand(command,
                     std::move(callback),
                     std::move(exceptionCallback),
                     args);
        va_end(args);
    }
    void sendvCommand(
        string_view command,
        std::function<void(const RedisResult &)> &&callback,
        std::function<void(const std::exception &)> &&exceptionCallback,
        va_list ap)
    {
        LOG_TRACE << "redis command: " << command;
        char *cmd;
        auto len = redisvFormatCommand(&cmd, command.data(), ap);
        if (len == -1)
        {
            exceptionCallback(std::runtime_error("Out of memory"));
            return;
        }
        else if (len == -2)
        {
            exceptionCallback(std::runtime_error("Invalid format string"));
            return;
        }
        else if (len <= 0)
        {
            exceptionCallback(std::runtime_error("Unknown format error"));
            return;
        }
        std::string fullCommand{cmd, static_cast<size_t>(len)};
        free(cmd);
        if (loop_->isInLoopThread())
        {
            sendCommandInloop(fullCommand,
                              std::move(callback),
                              std::move(exceptionCallback));
        }
        else
        {
            loop_->queueInLoop(
                [this,
                 callback = std::move(callback),
                 exceptionCallback = std::move(exceptionCallback),
                 fullCommand = std::move(fullCommand),
                 ap]() mutable {
                    sendCommandInloop(fullCommand,
                                      std::move(callback),
                                      std::move(exceptionCallback));
                });
        }
    }
    ~RedisConnection()
    {
        LOG_TRACE << (int)status_;
        if (redisContext_ && status_ != ConnectStatus::kEnd)
            redisAsyncDisconnect(redisContext_);
    }
    void disconnect();

  private:
    redisAsyncContext *redisContext_{nullptr};
    const trantor::InetAddress serverAddr_;
    const std::string password_;
    trantor::EventLoop *loop_{nullptr};
    std::unique_ptr<trantor::Channel> channel_{nullptr};
    std::function<void(std::shared_ptr<RedisConnection> &&)> connectCallback_;
    std::function<void(std::shared_ptr<RedisConnection> &&)>
        disconnectCallback_;
    std::queue<std::function<void(const RedisResult &)>> commandCallbacks_;
    std::queue<std::function<void(const std::exception &)>> exceptionCallbacks_;
    string_view command_;
    ConnectStatus status_{ConnectStatus::kNone};
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
        std::function<void(const std::exception &)> &&exceptionCallback);
    void handleDisconnect();
};
using RedisConnectionPtr = std::shared_ptr<RedisConnection>;
}  // namespace nosql
}  // namespace drogon