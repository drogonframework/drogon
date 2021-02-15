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
#include "RedisResultImpl.h"

using namespace drogon::nosql;
RedisConnection::RedisConnection(const trantor::InetAddress &serverAddress,
                                 const std::string &password,
                                 trantor::EventLoop *loop)
    : serverAddr_(serverAddress), password_(password), loop_(loop)
{
    assert(loop_);
    auto thisPtr = shared_from_this();
    loop_->queueInLoop([thisPtr]() { thisPtr->startConnectionInLoop(); });
}

void RedisConnection::startConnectionInLoop()
{
    loop_->assertInLoopThread();
    assert(!redisContext_);

    redisContext_ =
        ::redisAsyncConnect(serverAddr_.toIp().c_str(), serverAddr_.toPort());
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
            }
            else
            {
                LOG_TRACE << "Connected successfully to "
                          << thisPtr->serverAddr_.toIpPort();
                thisPtr->connected_ = true;
            }
            if (thisPtr->connectCallback_)
            {
                thisPtr->connectCallback_(status);
            }
        });
    redisAsyncSetDisconnectCallback(
        redisContext_, [](const redisAsyncContext *context, int status) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->connected_ = false;
            if (thisPtr->disconnectCallback_)
            {
                thisPtr->disconnectCallback_(status);
            }
            thisPtr->channel_->disableAll();
            thisPtr->channel_->remove();
            thisPtr->channel_.reset();
            LOG_TRACE << "Disconnected from "
                      << thisPtr->serverAddr_.toIpPort();
        });
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
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->disableAll();
}

void RedisConnection::handleRedisRead()
{
    redisAsyncHandleRead(redisContext_);
}
void RedisConnection::handleRedisWrite()
{
    if (!(redisContext_->c.flags & REDIS_CONNECTED))
    {
        channel_->disableAll();
        channel_->remove();
        channel_.reset();
    }
    redisAsyncHandleWrite(redisContext_);
}

void RedisConnection::sendCommand(
    const string_view &command,
    std::function<void(const RedisResult &)> &&callback,
    std::function<void(const std::exception &)> &&exceptionCallback,
    ...)
{
    commandCallback_ = std::move(callback);
    exceptionCallback_ = std::move(exceptionCallback);
    command_ = command;
    va_list args;
    va_start(args, exceptionCallback);
    redisAsyncCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void *userData) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleResult(static_cast<redisReply *>(r));
        },
        nullptr,
        command.data(),
        args);
    va_end(args);
}

void RedisConnection::handleResult(redisReply *result)
{
    if (result->type != REDIS_REPLY_ERROR)
    {
        commandCallback_(RedisResultImpl(result));
    }
    else
    {
        exceptionCallback_(std::runtime_error(result->str));
    }
}
