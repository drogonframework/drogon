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

using namespace drogon;
using namespace drogon::orm;

PgListener::PgListener(DbClientPtr dbClient) : dbClient_(std::move(dbClient))
{
}

DbClientPtr PgListener::dbClient() const
{
    return dbClient_;
}

void PgListener::listen(
    const std::string& channel,
    std::function<void(std::string, std::string)> messageCallback)
{
    // save message callback
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenChannels_[channel].push_back(std::move(messageCallback));
    }

    // TODO: escape special chars
    std::string sql = "LISTEN " + channel;
    dbClient_->execSqlAsync(
        sql,
        [channel](const Result& r) {
            LOG_DEBUG << "Subscribe success to " << channel;
        },
        [channel](const DrogonDbException& ex) {
            LOG_ERROR << "Failed to subscribe to " << channel
                      << ", error: " << ex.base().what();
            // TODO: keep trying?
        });
}

void PgListener::unlisten(const std::string& channel) noexcept
{
    // delete callbacks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenChannels_.erase(channel);
    }

    // TODO: escape special chars
    std::string sql = "UNLISTEN " + channel;
    dbClient_->execSqlAsync(
        sql,
        [channel](const Result& r) { LOG_DEBUG << "Unlisten " << channel; },
        [channel](const DrogonDbException& ex) {
            LOG_ERROR << "Failed to unlisten " << channel
                      << ", error: " << ex.base().what();
            // TODO: keep trying?
        });
}

void PgListener::onMessage(const std::string& channel,
                           const std::string& message,
                           trantor::EventLoop* loop) const
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
            loop->queueInLoop(
                [cb, channel, message]() { cb(channel, message); });
        }
    }
}
