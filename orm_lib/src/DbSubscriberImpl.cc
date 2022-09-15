//
// Created by wanchen.he on 2022/9/14.
//

#include "DbSubscriberImpl.h"
#include "DbConnection.h"
#include <trantor/utils/Logger.h>

using namespace drogon::orm;

DbSubscriberImpl::~DbSubscriberImpl()
{
    DbConnectionPtr conn;
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        conn.swap(conn_);
        conn->loop()->runInLoop([conn]() {
            // Run in self loop to avoid blocking
            conn->disconnect();
        });
    }
}

void DbSubscriberImpl::subscribe(const std::string &channel,
                                 MessageCallback &&messageCallback) noexcept
{
    LOG_TRACE << "Subscribe " << channel;

    std::shared_ptr<DbSubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (subContexts_.find(channel) != subContexts_.end())
        {
            subCtx = subContexts_.at(channel);
        }
        else
        {
            subCtx = std::make_shared<DbSubscribeContext>(shared_from_this(),
                                                          channel);
            subContexts_.emplace(channel, subCtx);
        }
        subCtx->addMessageCallback(std::move(messageCallback));
    }

    DbConnectionPtr connPtr;
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

void DbSubscriberImpl::unsubscribe(const std::string &channel) noexcept
{
    LOG_TRACE << "Unsubscribe " << channel;

    std::shared_ptr<DbSubscribeContext> subCtx;
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

    DbConnectionPtr connPtr;
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

void DbSubscriberImpl::setConnection(const DbConnectionPtr &conn)
{
    assert(conn);
    std::lock_guard<std::mutex> lock(mutex_);
    assert(!conn_);
    conn_ = conn;
}

void DbSubscriberImpl::clearConnection()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        conn_ = nullptr;
        tasks_.clear();
    }
}

void DbSubscriberImpl::subscribeNext()
{
    DbConnectionPtr connPtr;
    std::shared_ptr<std::function<void(const DbConnectionPtr &)>> taskPtr;
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

void DbSubscriberImpl::subscribeAll()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &item : subContexts_)
        {
            auto subCtx = item.second;
            tasks_.emplace_back(
                std::make_shared<std::function<void(const DbConnectionPtr &)>>(
                    [subCtx](const DbConnectionPtr &connPtr) mutable {
                        connPtr->sendSubscribe(subCtx);
                    }));
        }
    }
    subscribeNext();
}
