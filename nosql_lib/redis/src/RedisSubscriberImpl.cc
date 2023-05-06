/**
 *
 *  @file RedisSubscriberImpl.cpp
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "RedisSubscriberImpl.h"

using namespace drogon::nosql;

RedisSubscriberImpl::~RedisSubscriberImpl()
{
    RedisConnectionPtr conn;
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        conn.swap(conn_);
        conn->getLoop()->runInLoop([conn]() {
            // Run in self loop to avoid blocking
            conn->disconnect();
        });
    }
}

void RedisSubscriberImpl::subscribe(
    const std::string &channel,
    RedisMessageCallback &&messageCallback) noexcept
{
    LOG_TRACE << "Subscribe " << channel;

    std::shared_ptr<SubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (subContexts_.find(channel) != subContexts_.end())
        {
            subCtx = subContexts_.at(channel);
        }
        else
        {
            subCtx = SubscribeContext::newContext(shared_from_this(), channel);
            subContexts_.emplace(channel, subCtx);
        }
        subCtx->addMessageCallback(std::move(messageCallback));
    }

    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connPtr = conn_;
    }

    if (connPtr)
    {
        connPtr->sendSubscribe(subCtx);
    }
    else
    {
        LOG_TRACE << "no subscribe connection available, wait for connection";
        // Just wait for connection, all channels will be re-sub
    }
}

void RedisSubscriberImpl::psubscribe(
    const std::string &pattern,
    RedisMessageCallback &&messageCallback) noexcept
{
    LOG_TRACE << "Psubscribe " << pattern;

    std::shared_ptr<SubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (psubContexts_.find(pattern) != psubContexts_.end())
        {
            subCtx = psubContexts_.at(pattern);
        }
        else
        {
            subCtx =
                SubscribeContext::newContext(shared_from_this(), pattern, true);
            psubContexts_.emplace(pattern, subCtx);
        }
        subCtx->addMessageCallback(std::move(messageCallback));
    }

    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connPtr = conn_;
    }

    if (connPtr)
    {
        connPtr->sendSubscribe(subCtx);
    }
    else
    {
        LOG_TRACE << "no subscribe connection available, wait for connection";
        // Just wait for connection, all channels will be re-sub
    }
}

void RedisSubscriberImpl::unsubscribe(const std::string &channel) noexcept
{
    LOG_TRACE << "Unsubscribe " << channel;

    std::shared_ptr<SubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = subContexts_.find(channel);
        if (iter == subContexts_.end())
        {
            LOG_DEBUG << "Attempt to unsubscribe from unknown channel "
                      << channel;
            return;
        }
        subCtx = std::move(iter->second);
        subContexts_.erase(iter);
    }
    subCtx->disable();

    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connPtr = conn_;
    }
    if (!connPtr)
    {
        LOG_TRACE << "Connection unavailable, no need to send unsub command";
        return;
    }

    connPtr->sendUnsubscribe(subCtx);
}

void RedisSubscriberImpl::punsubscribe(const std::string &pattern) noexcept
{
    LOG_TRACE << "Punsubscribe " << pattern;

    std::shared_ptr<SubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = psubContexts_.find(pattern);
        if (iter == psubContexts_.end())
        {
            LOG_DEBUG << "Attempt to punsubscribe from unknown pattern "
                      << pattern;
            return;
        }
        subCtx = std::move(iter->second);
        psubContexts_.erase(iter);
    }
    subCtx->disable();

    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connPtr = conn_;
    }
    if (!connPtr)
    {
        LOG_TRACE << "Connection unavailable, no need to send unsub command";
        return;
    }

    connPtr->sendUnsubscribe(subCtx);
}

void RedisSubscriberImpl::setConnection(const RedisConnectionPtr &conn)
{
    assert(conn);
    std::lock_guard<std::mutex> lock(mutex_);
    assert(!conn_);
    conn_ = conn;
}

void RedisSubscriberImpl::clearConnection()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        conn_ = nullptr;
        tasks_.clear();
    }
}

void RedisSubscriberImpl::subscribeNext()
{
    RedisConnectionPtr connPtr;
    std::shared_ptr<std::function<void(const RedisConnectionPtr &)>> taskPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!conn_ || tasks_.empty())
        {
            return;
        }
        connPtr = conn_;
        taskPtr = std::move(tasks_.front());
        tasks_.pop_front();
    }
    (*taskPtr)(connPtr);
}

void RedisSubscriberImpl::subscribeAll()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &item : subContexts_)
        {
            auto subCtx = item.second;
            tasks_.emplace_back(
                std::make_shared<
                    std::function<void(const RedisConnectionPtr &)>>(
                    [subCtx](const RedisConnectionPtr &connPtr) mutable {
                        connPtr->sendSubscribe(subCtx);
                    }));
        }
        for (auto &item : psubContexts_)
        {
            auto subCtx = item.second;
            tasks_.emplace_back(
                std::make_shared<
                    std::function<void(const RedisConnectionPtr &)>>(
                    [subCtx](const RedisConnectionPtr &connPtr) mutable {
                        connPtr->sendSubscribe(subCtx);
                    }));
        }
    }
    subscribeNext();
}
