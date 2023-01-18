/**
 *
 *  @file PgListener.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, An Tao.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "PgListener.h"
#include "PgConnection.h"

using namespace drogon;
using namespace drogon::orm;

#define MAX_UNLISTEN_RETRY 3
#define MAX_LISTEN_RETRY 10

PgListener::PgListener(std::string connInfo, trantor::EventLoop* loop)
    : connectionInfo_(std::move(connInfo)), loop_(loop)
{
    if (!loop)
    {
        threadPtr_ = std::make_unique<trantor::EventLoopThread>();
        threadPtr_->run();
        loop_ = threadPtr_->getLoop();
    }
}

PgListener::~PgListener()
{
    if (conn_)
    {
        conn_->disconnect();
        conn_ = nullptr;
    }
}

void PgListener::init() noexcept
{
    // shared_from_this() can not be called in constructor
    std::weak_ptr<PgListener> weakThis = shared_from_this();
    loop_->queueInLoop([weakThis]() {
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
        {
            return;
        }
        thisPtr->connHolder_ = thisPtr->newConnection();
    });
}

void PgListener::listen(
    const std::string& channel,
    std::function<void(std::string, std::string)> messageCallback) noexcept
{
    if (loop_->isInLoopThread())
    {
        listenChannels_[channel].push_back(std::move(messageCallback));
        listenInLoop(channel, true);
    }
    else
    {
        std::weak_ptr<PgListener> weakThis = shared_from_this();
        loop_->queueInLoop(
            [weakThis, channel, cb = std::move(messageCallback)]() mutable {
                auto thisPtr = weakThis.lock();
                if (!thisPtr)
                {
                    return;
                }
                thisPtr->listenChannels_[channel].push_back(std::move(cb));
                thisPtr->listenInLoop(channel, true);
            });
    }
}

void PgListener::unlisten(const std::string& channel) noexcept
{
    if (loop_->isInLoopThread())
    {
        listenChannels_.erase(channel);
        listenInLoop(channel, false);
    }
    else
    {
        std::weak_ptr<PgListener> weakThis = shared_from_this();
        loop_->queueInLoop([weakThis, channel]() {
            auto thisPtr = weakThis.lock();
            if (!thisPtr)
            {
                return;
            }
            thisPtr->listenChannels_.erase(channel);
            thisPtr->listenInLoop(channel, false);
        });
    }
}

void PgListener::onMessage(const std::string& channel,
                           const std::string& message) const noexcept
{
    loop_->assertInLoopThread();

    auto iter = listenChannels_.find(channel);
    if (iter == listenChannels_.end())
    {
        return;
    }
    for (auto& cb : iter->second)
    {
        cb(channel, message);
    }
}

void PgListener::listenAll() noexcept
{
    loop_->assertInLoopThread();

    listenTasks_.clear();
    for (auto& item : listenChannels_)
    {
        listenTasks_.emplace_back(true, item.first);
    }
    listenNext();
}

void PgListener::listenNext() noexcept
{
    loop_->assertInLoopThread();

    if (listenTasks_.empty())
    {
        return;
    }
    auto [listen, channel] = listenTasks_.front();
    listenTasks_.pop_front();
    listenInLoop(channel, listen);
}

void PgListener::listenInLoop(const std::string& channel,
                              bool listen,
                              std::shared_ptr<unsigned int> retryCnt)
{
    loop_->assertInLoopThread();
    if (!retryCnt)
        retryCnt = std::make_shared<unsigned int>(0);
    if (conn_ && !conn_->isWorking())
    {
        auto pgConn = std::dynamic_pointer_cast<PgConnection>(conn_);
        std::string escapedChannel =
            escapeIdentifier(pgConn, channel.c_str(), channel.size());
        if (escapedChannel.empty())
        {
            LOG_ERROR << "Failed to escape pg identifier, stop listen";
            // TODO: report
            return;
        }

        // Because DbConnection::execSql() takes string_view as parameter,
        // sql must be hold until query finish.
        auto sql = std::make_shared<std::string>(
            (listen ? "LISTEN " : "UNLISTEN ") + escapedChannel);
        std::weak_ptr<PgListener> weakThis = shared_from_this();
        conn_->execSql(
            *sql,
            0,
            {},
            {},
            {},
            [listen, channel, sql](const Result& r) {
                if (listen)
                {
                    LOG_TRACE << "Listen channel " << channel;
                }
                else
                {
                    LOG_TRACE << "Unlisten channel " << channel;
                }
            },
            [listen, channel, weakThis, sql, retryCnt, loop = loop_](
                const std::exception_ptr& exception) {
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (const DrogonDbException& ex)
                {
                    ++(*retryCnt);
                    if (listen)
                    {
                        LOG_ERROR << "Failed to listen channel " << channel
                                  << ", error: " << ex.base().what();
                        if (*retryCnt > MAX_LISTEN_RETRY)
                        {
                            LOG_ERROR << "Failed to listen channel " << channel
                                      << " after max attempt. Stop trying.";
                            // TODO: report
                            return;
                        }
                    }
                    else
                    {
                        LOG_ERROR << "Failed to unlisten channel " << channel
                                  << ", error: " << ex.base().what();
                        if (*retryCnt > MAX_UNLISTEN_RETRY)
                        {
                            LOG_ERROR << "Failed to unlisten channel "
                                      << channel
                                      << " after max attempt. Stop trying.";
                            // TODO: report?
                            return;
                        }
                    }
                    auto delay = (*retryCnt) < 5 ? (*retryCnt * 2) : 10;
                    loop->runAfter(delay, [=]() {
                        auto thisPtr = weakThis.lock();
                        if (thisPtr)
                        {
                            thisPtr->listenInLoop(channel, listen, retryCnt);
                        }
                    });
                }
            });
        return;
    }

    if (listenTasks_.size() > 20000)
    {
        LOG_WARN << "Too many queries in listen buffer. Stop listen channel "
                 << channel;
        // TODO: report
        return;
    }

    LOG_TRACE << "Add to task queue, channel " << channel;
    listenTasks_.emplace_back(listen, channel);
}

PgConnectionPtr PgListener::newConnection(
    std::shared_ptr<unsigned int> retryCnt)
{
    PgConnectionPtr connPtr =
        std::make_shared<PgConnection>(loop_, connectionInfo_, false);
    std::weak_ptr<PgListener> weakPtr = shared_from_this();
    if (!retryCnt)
        retryCnt = std::make_shared<unsigned int>(0);
    connPtr->setCloseCallback(
        [weakPtr, retryCnt](const DbConnectionPtr& closeConnPtr) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            // Erase the connection
            if (closeConnPtr == thisPtr->conn_)
            {
                thisPtr->conn_.reset();
            }
            if (closeConnPtr == thisPtr->connHolder_)
            {
                thisPtr->connHolder_.reset();
            }
            // Reconnect after delay
            ++(*retryCnt);
            unsigned int delay = (*retryCnt) < 5 ? (*retryCnt * 2) : 10;
            thisPtr->loop_->runAfter(delay, [weakPtr, closeConnPtr, retryCnt] {
                auto thisPtr = weakPtr.lock();
                if (!thisPtr)
                    return;
                assert(!thisPtr->connHolder_);
                thisPtr->connHolder_ = thisPtr->newConnection(retryCnt);
            });
        });
    connPtr->setOkCallback(
        [weakPtr, retryCnt](const DbConnectionPtr& okConnPtr) {
            LOG_TRACE << "connected after " << *retryCnt << " tries";
            (*retryCnt) = 0;
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            assert(!thisPtr->conn_);
            assert(thisPtr->connHolder_ == okConnPtr);
            thisPtr->conn_ = okConnPtr;
            thisPtr->listenAll();
        });
    connPtr->setIdleCallback([weakPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        thisPtr->listenNext();
    });

    connPtr->setMessageCallback(
        [weakPtr](const std::string& channel, const std::string& message) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                thisPtr->onMessage(channel, message);
            }
        });
    return connPtr;
}

std::string PgListener::escapeIdentifier(const PgConnectionPtr& conn,
                                         const char* str,
                                         size_t length)
{
    auto res = std::unique_ptr<char, std::function<void(char*)>>(
        PQescapeIdentifier(conn->pgConn().get(), str, length), [](char* res) {
            if (res)
            {
                PQfreemem(res);
            }
        });
    if (!res)
    {
        LOG_ERROR << "Error when escaping identifier ["
                  << std::string(str, length) << "]. "
                  << PQerrorMessage(conn->pgConn().get());
        return {};
    }
    return std::string{res.get()};
}
