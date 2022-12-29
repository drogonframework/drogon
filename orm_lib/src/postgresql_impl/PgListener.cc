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
#include "../DbClientLockFree.h"
#include <drogon/orm/DbClient.h>

using namespace drogon;
using namespace drogon::orm;

PgListener::PgListener(trantor::EventLoop* loop)
{
    if (loop)
    {
        loop_ = loop;
    }
    else
    {
        threadPtr_ = std::make_unique<trantor::EventLoopThread>();
        threadPtr_->run();
        loop_ = threadPtr_->getLoop();
    }
}

PgListener::~PgListener()
{
    // Need unlisten all?
}

void PgListener::setDbClient(DbClientPtr dbClient)
{
    dbClient_ = std::move(dbClient);
}

void PgListener::listen(
    const std::string& channel,
    std::function<void(std::string, std::string)> messageCallback) noexcept
{
    // save message callback
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenChannels_[channel].push_back(std::move(messageCallback));
    }

    doListen(channel);
}

void PgListener::unlisten(const std::string& channel) noexcept
{
    // delete callbacks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenChannels_.erase(channel);
    }

    std::weak_ptr<PgListener> weakThis = shared_from_this();
    loop_->runInLoop([channel, weakThis]() {
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
        {
            return;
        }
        std::string sql = formatUnlistenCommand(channel);
        thisPtr->dbClient_->execSqlAsync(
            sql,
            [channel](const Result& r) { LOG_DEBUG << "Unlisten " << channel; },
            [channel](const DrogonDbException& ex) {
                LOG_ERROR << "Failed to unlisten " << channel
                          << ", error: " << ex.base().what();
                // TODO: keep trying?
            });
    });
}

void PgListener::onMessage(const std::string& channel,
                           const std::string& message) const noexcept
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = listenChannels_.find(channel);
        if (iter == listenChannels_.end())
        {
            return;
        }
        for (auto& cb : iter->second)
        {
            loop_->queueInLoop(
                [cb, channel, message]() { cb(channel, message); });
        }
    }
}

void PgListener::listenAll() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : listenChannels_)
    {
        std::string channel = item.first;
        doListen(channel);
    }
}

std::string PgListener::formatListenCommand(const std::string& channel)
{
    // TODO: escape special chars
    return "LISTEN " + channel;
}

std::string PgListener::formatUnlistenCommand(const std::string& channel)
{
    // TODO: escape special chars
    return "UNLISTEN " + channel;
}

void PgListener::doListen(const std::string& channel)
{
    if (loop_->isInLoopThread())
    {
        doListenInLoop(channel);
    }
    else
    {
        std::weak_ptr<PgListener> weakThis = shared_from_this();
        loop_->queueInLoop([weakThis, channel]() {
            auto thisPtr = weakThis.lock();
            if (thisPtr)
            {
                thisPtr->doListenInLoop(channel);
            }
        });
    }
}

void PgListener::doListenInLoop(const std::string& channel)
{
    std::string sql = formatListenCommand(channel);
    std::weak_ptr<PgListener> weakThis = shared_from_this();
    dbClient_->execSqlAsync(
        sql,
        [channel](const Result& r) {
            LOG_DEBUG << "Listen channel " << channel;
        },
        [channel, weakThis, loop = loop_](const DrogonDbException& ex) {
            LOG_ERROR << "Failed to listen channel " << channel
                      << ", error: " << ex.base().what();
            // TODO: max retry?
            loop->runAfter(1.0, [channel, weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                    thisPtr->doListenInLoop(channel);
                }
            });
        });
}
