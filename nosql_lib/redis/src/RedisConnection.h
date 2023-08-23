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
#include <string_view>
#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisResult.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <hiredis/async.h>
#include <hiredis/hiredis.h>
#include <memory>
#include <queue>

#include "SubscribeContext.h"

namespace drogon
{
namespace nosql
{
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
                    const std::string &username,
                    const std::string &password,
                    unsigned int db,
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

    void setIdleCallback(
        const std::function<void(const std::shared_ptr<RedisConnection> &)>
            &callback)
    {
        idleCallback_ = callback;
    }

    static std::string getFormattedCommand(const std::string_view &command,
                                           va_list ap) noexcept(false)
    {
        char *cmd;
        auto len = redisvFormatCommand(&cmd, command.data(), ap);
        if (len == -1)
        {
            throw RedisException(RedisErrorCode::kInternalError,
                                 "Out of memory");
        }
        else if (len == -2)
        {
            throw RedisException(RedisErrorCode::kInternalError,
                                 "Invalid format string");
        }
        else if (len <= 0)
        {
            throw RedisException(RedisErrorCode::kInternalError,
                                 "Unknown format error");
        }
        std::string fullCommand{cmd, static_cast<size_t>(len)};
        free(cmd);
        return fullCommand;
    }

    void sendFormattedCommand(std::string &&command,
                              RedisResultCallback &&resultCallback,
                              RedisExceptionCallback &&exceptionCallback)
    {
        if (loop_->isInLoopThread())
        {
            sendCommandInLoop(command,
                              std::move(resultCallback),
                              std::move(exceptionCallback));
        }
        else
        {
            loop_->queueInLoop(
                [this,
                 callback = std::move(resultCallback),
                 exceptionCallback = std::move(exceptionCallback),
                 command = std::move(command)]() mutable {
                    sendCommandInLoop(command,
                                      std::move(callback),
                                      std::move(exceptionCallback));
                });
        }
    }

    void sendvCommand(std::string_view command,
                      RedisResultCallback &&resultCallback,
                      RedisExceptionCallback &&exceptionCallback,
                      va_list ap)
    {
        LOG_TRACE << "redis command: " << command;
        try
        {
            auto fullCommand = getFormattedCommand(command, ap);
            if (loop_->isInLoopThread())
            {
                sendCommandInLoop(fullCommand,
                                  std::move(resultCallback),
                                  std::move(exceptionCallback));
            }
            else
            {
                loop_->queueInLoop(
                    [this,
                     callback = std::move(resultCallback),
                     exceptionCallback = std::move(exceptionCallback),
                     fullCommand = std::move(fullCommand)]() mutable {
                        sendCommandInLoop(fullCommand,
                                          std::move(callback),
                                          std::move(exceptionCallback));
                    });
            }
        }
        catch (const RedisException &err)
        {
            exceptionCallback(err);
        }
    }

    void sendSubscribe(const std::shared_ptr<SubscribeContext> &subCtx);
    void sendUnsubscribe(const std::shared_ptr<SubscribeContext> &subCtx);

    ~RedisConnection()
    {
        LOG_TRACE << (int)status_;
        if (redisContext_ && status_ != ConnectStatus::kEnd)
            redisAsyncDisconnect(redisContext_);
    }

    void disconnect();

    void sendCommand(RedisResultCallback &&resultCallback,
                     RedisExceptionCallback &&exceptionCallback,
                     std::string_view command,
                     ...)
    {
        va_list args;
        va_start(args, command);
        sendvCommand(command,
                     std::move(resultCallback),
                     std::move(exceptionCallback),
                     args);
        va_end(args);
    }

    trantor::EventLoop *getLoop() const
    {
        return loop_;
    }

  private:
    redisAsyncContext *redisContext_{nullptr};
    const trantor::InetAddress serverAddr_;
    const std::string username_;
    const std::string password_;
    const unsigned int db_;
    trantor::EventLoop *loop_{nullptr};
    std::unique_ptr<trantor::Channel> channel_{nullptr};
    std::function<void(std::shared_ptr<RedisConnection> &&)> connectCallback_;
    std::function<void(std::shared_ptr<RedisConnection> &&)>
        disconnectCallback_;
    std::function<void(const std::shared_ptr<RedisConnection> &)> idleCallback_;
    std::queue<RedisResultCallback> resultCallbacks_;
    std::queue<RedisExceptionCallback> exceptionCallbacks_;
    ConnectStatus status_{ConnectStatus::kNone};

    // used to keep the lifetime of context object
    std::unordered_map<unsigned long long, std::shared_ptr<SubscribeContext>>
        subContexts_;

    void startConnectionInLoop();
    static void addWrite(void *userData);
    static void delWrite(void *userData);
    static void addRead(void *userData);
    static void delRead(void *userData);
    static void cleanup(void *userData);
    void handleRedisRead();
    void handleRedisWrite();
    void handleResult(redisReply *result);
    void sendCommandInLoop(const std::string &command,
                           RedisResultCallback &&resultCallback,
                           RedisExceptionCallback &&exceptionCallback);
    void sendSubscribeInLoop(const std::shared_ptr<SubscribeContext> &subCtx);
    void sendUnsubscribeInLoop(const std::shared_ptr<SubscribeContext> &subCtx);
    void handleSubscribeResult(redisReply *result, SubscribeContext *subCtx);

    void handleDisconnect();
};

using RedisConnectionPtr = std::shared_ptr<RedisConnection>;
}  // namespace nosql
}  // namespace drogon
