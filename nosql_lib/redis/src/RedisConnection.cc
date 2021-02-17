/**
 *
 *  @file RedisConnection.cc
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

#include "RedisConnection.h"
#include <drogon/nosql/RedisResult.h>
#include <future>

using namespace drogon::nosql;
RedisConnection::RedisConnection(const trantor::InetAddress &serverAddress,
                                 const std::string &password,
                                 trantor::EventLoop *loop)
    : serverAddr_(serverAddress), password_(password), loop_(loop)
{
    assert(loop_);
    loop_->queueInLoop([this]() { startConnectionInLoop(); });
}

void RedisConnection::startConnectionInLoop()
{
    loop_->assertInLoopThread();
    assert(!redisContext_);

    redisContext_ =
        ::redisAsyncConnect(serverAddr_.toIp().c_str(), serverAddr_.toPort());
    status_ = ConnectStatus::kConnecting;
    if (redisContext_->err)
    {
        LOG_ERROR << "Error: " << redisContext_->errstr;
        if (disconnectCallback_)
        {
            disconnectCallback_(shared_from_this());
        }
    }
    redisContext_->ev.addWrite = addWrite;
    redisContext_->ev.delWrite = delWrite;
    redisContext_->ev.addRead = addRead;
    redisContext_->ev.delRead = delRead;
    redisContext_->ev.cleanup = cleanup;
    redisContext_->ev.data = this;

    channel_ = std::make_unique<trantor::Channel>(loop_, redisContext_->c.fd);
    channel_->setReadCallback([this]() { handleRedisRead(); });
    channel_->setWriteCallback([this]() { handleRedisWrite(); });
    redisAsyncSetConnectCallback(
        redisContext_, [](const redisAsyncContext *context, int status) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            if (status != REDIS_OK)
            {
                LOG_ERROR << "Failed to connect to "
                          << thisPtr->serverAddr_.toIpPort() << "! "
                          << context->errstr;
                thisPtr->handleDisconnect();
                if (thisPtr->disconnectCallback_)
                {
                    thisPtr->disconnectCallback_(thisPtr->shared_from_this());
                }
            }
            else
            {
                LOG_TRACE << "Connected successfully to "
                          << thisPtr->serverAddr_.toIpPort();
                if (thisPtr->password_.empty())
                {
                    thisPtr->status_ = ConnectStatus::kConnected;
                    if (thisPtr->connectCallback_)
                    {
                        thisPtr->connectCallback_(thisPtr->shared_from_this());
                    }
                }
                else
                {
                    std::weak_ptr<RedisConnection> weakThisPtr =
                        thisPtr->shared_from_this();
                    thisPtr->sendCommand(
                        "auth %s",
                        [weakThisPtr](const RedisResult &r) {
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            if (r.asString() == "OK")
                            {
                                if (thisPtr->connectCallback_)
                                    thisPtr->connectCallback_(
                                        thisPtr->shared_from_this());
                            }
                            else
                            {
                                LOG_ERROR << r.asString();
                                thisPtr->disconnect();
                            }
                        },
                        [weakThisPtr](const std::exception &err) {
                            LOG_ERROR << err.what();
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            thisPtr->disconnect();
                        },
                        thisPtr->password_.data());
                }
            }
        });
    redisAsyncSetDisconnectCallback(
        redisContext_, [](const redisAsyncContext *context, int status) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);

            thisPtr->handleDisconnect();
            if (thisPtr->disconnectCallback_)
            {
                thisPtr->disconnectCallback_(thisPtr->shared_from_this());
            }

            LOG_TRACE << "Disconnected from "
                      << thisPtr->serverAddr_.toIpPort();
        });
}

void RedisConnection::handleDisconnect()
{
    LOG_TRACE << "handleDisconnect";
    status_ = ConnectStatus::kEnd;
    channel_->disableAll();
    channel_->remove();
    redisContext_->ev.addWrite = nullptr;
    redisContext_->ev.delWrite = nullptr;
    redisContext_->ev.addRead = nullptr;
    redisContext_->ev.delRead = nullptr;
    redisContext_->ev.cleanup = nullptr;
    redisContext_->ev.data = nullptr;
}
void RedisConnection::addWrite(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->enableWriting();
}
void RedisConnection::delWrite(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->disableWriting();
}
void RedisConnection::addRead(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->enableReading();
}
void RedisConnection::delRead(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->disableReading();
}
void RedisConnection::cleanup(void *userData)
{
    LOG_TRACE << "cleanup";
}

void RedisConnection::handleRedisRead()
{
    redisAsyncHandleRead(redisContext_);
}
void RedisConnection::handleRedisWrite()
{
    if (redisContext_->c.flags == REDIS_DISCONNECTING)
    {
        channel_->disableAll();
        channel_->remove();
    }
    redisAsyncHandleWrite(redisContext_);
}

void RedisConnection::sendCommandInloop(
    const std::string &command,
    std::function<void(const RedisResult &)> &&callback,
    std::function<void(const std::exception &)> &&exceptionCallback)
{
    commandCallbacks_.emplace(std::move(callback));
    exceptionCallbacks_.emplace(std::move(exceptionCallback));
    command_ = command;

    redisAsyncFormattedCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void *userData) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleResult(static_cast<redisReply *>(r));
        },
        nullptr,
        command.c_str(),
        command.length());
}

void RedisConnection::handleResult(redisReply *result)
{
    LOG_TRACE << "redis reply: " << result->type;
    auto commandCallback = std::move(commandCallbacks_.front());
    commandCallbacks_.pop();
    auto exceptionCallback = std::move(exceptionCallbacks_.front());
    exceptionCallbacks_.pop();
    if (result->type != REDIS_REPLY_ERROR)
    {
        commandCallback(RedisResult(result));
    }
    else
    {
        exceptionCallback(
            std::runtime_error(std::string{result->str, result->len}));
    }
}

void RedisConnection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr, &pro]() {
        redisAsyncDisconnect(thisPtr->redisContext_);
        pro.set_value(1);
    });
    f.get();
}
