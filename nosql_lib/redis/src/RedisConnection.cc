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
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

using namespace drogon::nosql;

RedisConnection::RedisConnection(const trantor::InetAddress &serverAddress,
                                 const std::string &username,
                                 const std::string &password,
                                 unsigned int db,
                                 trantor::EventLoop *loop)
    : serverAddr_(serverAddress),
      username_(username),
      password_(password),
      db_(db),
      loop_(loop)
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

        // Strange things have happened. In some kinds of connection errors,
        // such as setsockopt errors, hiredis already set redisContext_->c.fd to
        // -1, but the tcp connection stays in ESTABLISHED status. And there is
        // no way for us to obtain the fd of that socket nor close it. This
        // probably is a bug of hiredis.
        return;
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
                    if (thisPtr->db_ == 0)
                    {
                        thisPtr->status_ = ConnectStatus::kConnected;
                        if (thisPtr->connectCallback_)
                        {
                            thisPtr->connectCallback_(
                                thisPtr->shared_from_this());
                        }
                    }
                }
                else
                {
                    if (thisPtr->username_.empty())
                    {
                        std::weak_ptr<RedisConnection> weakThisPtr =
                            thisPtr->shared_from_this();
                        thisPtr->sendCommand(
                            [weakThisPtr](const RedisResult &r) {
                                auto thisPtr = weakThisPtr.lock();
                                if (!thisPtr)
                                    return;
                                if (r.asString() == "OK")
                                {
                                    if (thisPtr->db_ == 0)
                                    {
                                        thisPtr->status_ =
                                            ConnectStatus::kConnected;
                                        if (thisPtr->connectCallback_)
                                            thisPtr->connectCallback_(
                                                thisPtr->shared_from_this());
                                    }
                                }
                                else
                                {
                                    LOG_ERROR << r.asString();
                                    thisPtr->disconnect();
                                    thisPtr->status_ = ConnectStatus::kEnd;
                                }
                            },
                            [weakThisPtr](const std::exception &err) {
                                LOG_ERROR << err.what();
                                auto thisPtr = weakThisPtr.lock();
                                if (!thisPtr)
                                    return;
                                thisPtr->disconnect();
                                thisPtr->status_ = ConnectStatus::kEnd;
                            },
                            "auth %s",
                            thisPtr->password_.c_str());
                    }
                    else
                    {
                        std::weak_ptr<RedisConnection> weakThisPtr =
                            thisPtr->shared_from_this();
                        thisPtr->sendCommand(
                            [weakThisPtr](const RedisResult &r) {
                                auto thisPtr = weakThisPtr.lock();
                                if (!thisPtr)
                                    return;
                                if (r.asString() == "OK")
                                {
                                    if (thisPtr->db_ == 0)
                                    {
                                        thisPtr->status_ =
                                            ConnectStatus::kConnected;
                                        if (thisPtr->connectCallback_)
                                            thisPtr->connectCallback_(
                                                thisPtr->shared_from_this());
                                    }
                                }
                                else
                                {
                                    LOG_ERROR << r.asString();
                                    thisPtr->disconnect();
                                    thisPtr->status_ = ConnectStatus::kEnd;
                                }
                            },
                            [weakThisPtr](const std::exception &err) {
                                LOG_ERROR << err.what();
                                auto thisPtr = weakThisPtr.lock();
                                if (!thisPtr)
                                    return;
                                thisPtr->disconnect();
                                thisPtr->status_ = ConnectStatus::kEnd;
                            },
                            "auth %s %s",
                            thisPtr->username_.c_str(),
                            thisPtr->password_.c_str());
                    }
                }

                if (thisPtr->db_ != 0)
                {
                    LOG_TRACE << "redis db:" << thisPtr->db_;
                    std::weak_ptr<RedisConnection> weakThisPtr =
                        thisPtr->shared_from_this();
                    thisPtr->sendCommand(
                        [weakThisPtr](const RedisResult &r) {
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            if (r.asString() == "OK")
                            {
                                thisPtr->status_ = ConnectStatus::kConnected;
                                if (thisPtr->connectCallback_)
                                {
                                    thisPtr->connectCallback_(
                                        thisPtr->shared_from_this());
                                }
                            }
                            else
                            {
                                LOG_ERROR << r.asString();
                                thisPtr->disconnect();
                                thisPtr->status_ = ConnectStatus::kEnd;
                            }
                        },
                        [weakThisPtr](const std::exception &err) {
                            LOG_ERROR << err.what();
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            thisPtr->disconnect();
                            thisPtr->status_ = ConnectStatus::kEnd;
                        },
                        "select %u",
                        thisPtr->db_);
                }
            }
        });
    redisAsyncSetDisconnectCallback(
        redisContext_, [](const redisAsyncContext *context, int /*status*/) {
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
    loop_->assertInLoopThread();
    while ((!resultCallbacks_.empty()) && (!exceptionCallbacks_.empty()))
    {
        if (exceptionCallbacks_.front())
        {
            exceptionCallbacks_.front()(
                RedisException(RedisErrorCode::kConnectionBroken,
                               "Connection is broken"));
        }
        resultCallbacks_.pop();
        exceptionCallbacks_.pop();
    }
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

void RedisConnection::cleanup(void * /*userData*/)
{
    LOG_TRACE << "cleanup";
}

void RedisConnection::handleRedisRead()
{
    if (status_ != ConnectStatus::kEnd)
    {
        redisAsyncHandleRead(redisContext_);
    }
}

void RedisConnection::handleRedisWrite()
{
    if (status_ != ConnectStatus::kEnd)
    {
        redisAsyncHandleWrite(redisContext_);
    }
}

void RedisConnection::sendCommandInLoop(
    const std::string &command,
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback)
{
    resultCallbacks_.emplace(std::move(resultCallback));
    exceptionCallbacks_.emplace(std::move(exceptionCallback));

    redisAsyncFormattedCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void * /*userData*/) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleResult(static_cast<redisReply *>(r));
        },
        nullptr,
        command.c_str(),
        command.length());
}

void RedisConnection::handleResult(redisReply *result)
{
    auto commandCallback = std::move(resultCallbacks_.front());
    resultCallbacks_.pop();
    auto exceptionCallback = std::move(exceptionCallbacks_.front());
    exceptionCallbacks_.pop();
    if (result && result->type != REDIS_REPLY_ERROR)
    {
        commandCallback(RedisResult(result));
    }
    else
    {
        if (result)
        {
            exceptionCallback(
                RedisException(RedisErrorCode::kRedisError,
                               std::string{result->str, result->len}));
        }
        else
        {
            exceptionCallback(RedisException(RedisErrorCode::kConnectionBroken,
                                             "Network failure"));
        }
    }
    if (resultCallbacks_.empty())
    {
        assert(exceptionCallbacks_.empty());
        if (idleCallback_)
        {
            idleCallback_(shared_from_this());
        }
    }
}

void RedisConnection::disconnect()
{
    auto thisPtr = shared_from_this();
    loop_->queueInLoop(
        [thisPtr]() { redisAsyncDisconnect(thisPtr->redisContext_); });
}

void RedisConnection::sendSubscribe(
    const std::shared_ptr<SubscribeContext> &subCtx)
{
    if (loop_->isInLoopThread())
    {
        sendSubscribeInLoop(subCtx);
    }
    else
    {
        loop_->queueInLoop([this, subCtx]() { sendSubscribeInLoop(subCtx); });
    }
}

void RedisConnection::sendUnsubscribe(
    const std::shared_ptr<SubscribeContext> &subCtx)
{
    if (loop_->isInLoopThread())
    {
        sendUnsubscribeInLoop(subCtx);
    }
    else
    {
        loop_->queueInLoop([this, subCtx]() { sendUnsubscribeInLoop(subCtx); });
    }
}

void RedisConnection::sendSubscribeInLoop(
    const std::shared_ptr<SubscribeContext> &subCtx)
{
    if (!subCtx->alive())
    {
        // Unsub-ed by somewhere else
        return;
    }
    subContexts_.emplace(subCtx->contextId(), subCtx);
    redisAsyncFormattedCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void *subCtx) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleSubscribeResult(static_cast<redisReply *>(r),
                                           static_cast<SubscribeContext *>(
                                               subCtx));
        },
        subCtx.get(),
        subCtx->subscribeCommand().c_str(),
        subCtx->subscribeCommand().size());
}

void RedisConnection::sendUnsubscribeInLoop(
    const std::shared_ptr<SubscribeContext> &subCtx)
{
    // There is a Hiredis issue here
    // The un-sub callback will not be called, sub callback will be called
    // instead, with first element in result as "unsubscribe".
    // This problem is fixed in 2021-12-02 commit da5a4ff, but
    // have not been released as a tag.
    // Here we just register a same function to deal with both situation.
    redisAsyncFormattedCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void *subCtx) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleSubscribeResult(static_cast<redisReply *>(r),
                                           static_cast<SubscribeContext *>(
                                               subCtx));
        },
        subCtx.get(),
        subCtx->unsubscribeCommand().c_str(),
        subCtx->unsubscribeCommand().size());
}

void RedisConnection::handleSubscribeResult(redisReply *result,
                                            SubscribeContext *subCtx)
{
    if (result && result->type == REDIS_REPLY_ARRAY && result->elements >= 3 &&
        result->element[0]->type == REDIS_REPLY_STRING)
    {
        const char *type = result->element[0]->str;
        int isPattern = (type[0] == 'p' || type[0] == 'P') ? 1 : 0;
        if (isPattern)
        {
            type += 1;
        }
        if (strcasecmp(type, "message") == 0)
        {
            std::string channel(result->element[1 + isPattern]->str,
                                result->element[1 + isPattern]->len);
            std::string message(result->element[2 + isPattern]->str,
                                result->element[2 + isPattern]->len);
            if (!subCtx->alive())
            {
                LOG_DEBUG << "Subscribe callback receive message, but "
                             "context is no "
                             "longer alive"
                          << ", channel: " << channel
                          << ", message: " << message;
            }
            else
            {
                subCtx->onMessage(channel, message);
            }
            // Message callback, no need to call idleCallback_
            return;
        }

        std::string channel(result->element[1]->str, result->element[1]->len);
        long long number = result->element[2]->integer;

        // On channel subscribed
        if (strcasecmp(type, "subscribe") == 0)
        {
            subCtx->onSubscribe(channel, number);
        }
        // On channel unsubscribed
        else if (strcasecmp(type, "unsubscribe") == 0)
        {
            subCtx->onUnsubscribe(channel, number);
            subContexts_.erase(subCtx->contextId());
        }
        // Should not happen
        else
        {
            LOG_ERROR << "Unknown redis response: " << result->element[0]->str;
            // Shouldn't let message from another endpoint to abort this
            // program. So no assert(false) here.
        }
    }
    else if (!result)
    {
        // When connection close, if a channel has been subscribed,
        // this callback will be called with empty result.
        LOG_DEBUG << "Empty result (connection lost)";
    }
    else if (result->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR << "Subscribe callback receive error result: " << result->str;
    }
    else
    {
        LOG_ERROR << "Subscribe callback receive error result type: "
                  << result->type;
    }

    if (idleCallback_)
    {
        idleCallback_(shared_from_this());
    }
}
