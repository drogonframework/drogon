/**
 *
 *  @file SubscribeContext.h
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
#pragma once

#include <drogon/nosql/RedisClient.h>
#include <list>
#include <mutex>
#include <utility>

namespace drogon::nosql
{
class SubscribeContext
{
  public:
    static std::shared_ptr<SubscribeContext> newContext(
        std::weak_ptr<RedisSubscriber>&& weakSub,
        const std::string& channel)
    {
        return std::shared_ptr<SubscribeContext>(
            new SubscribeContext(std::move(weakSub), channel));
    }

    const std::string& channel() const
    {
        return channel_;
    }

    const std::string& subscribeCommand() const
    {
        return subscribeCommand_;
    }

    const std::string& unsubscribeCommand() const
    {
        return unsubscribeCommand_;
    }

    void addMessageCallback(RedisMessageCallback&& messageCallback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.emplace_back(std::move(messageCallback));
    }

    void disable()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_ = true;
        messageCallbacks_.clear();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.clear();
    }

    void onMessage(const std::string& channel, const std::string& message);
    void onSubscribe();
    void onUnsubscribe();

    bool alive() const
    {
        return !disabled_ && weakSub_.lock() != nullptr;
    }

  private:
    explicit SubscribeContext(std::weak_ptr<RedisSubscriber>&& weakSub,
                              const std::string& channel);

    std::weak_ptr<RedisSubscriber> weakSub_;
    std::string channel_;
    std::string subscribeCommand_;
    std::string unsubscribeCommand_;
    std::mutex mutex_;
    std::list<RedisMessageCallback> messageCallbacks_;
    bool disabled_{false};
};

}  // namespace drogon::nosql
