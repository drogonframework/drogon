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
#include <atomic>
#include <mutex>
#include <utility>

namespace drogon::nosql
{
class SubscribeContext
{
  public:
    static std::shared_ptr<SubscribeContext> newContext(
        std::weak_ptr<RedisSubscriber>&& weakSub,
        const std::string& channel,
        bool isPattern = false)
    {
        return std::shared_ptr<SubscribeContext>(
            new SubscribeContext(std::move(weakSub), channel, isPattern));
    }

    unsigned long long contextId() const
    {
        return contextId_;
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

    /**
     * Message callback called by RedisConnection, whenever a message is
     * published in target channel
     * @param channel : target channel name
     * @param message : message from channel
     */
    void onMessage(const std::string& channel, const std::string& message);

    /**
     * Callback called by RedisConnection, whenever a sub or re-sub is success
     */
    void onSubscribe(const std::string& channel, long long numChannels);

    /**
     * Callback called by RedisConnection, when unsubscription success.
     */
    void onUnsubscribe(const std::string& channel, long long numChannels);

    bool alive() const
    {
        return !disabled_ && weakSub_.lock() != nullptr;
    }

  private:
    SubscribeContext(std::weak_ptr<RedisSubscriber>&& weakSub,
                     const std::string& channel,
                     bool isPattern);
    static std::atomic<unsigned long long> maxContextId_;

    unsigned long long contextId_;
    std::weak_ptr<RedisSubscriber> weakSub_;
    std::string channel_;
    bool isPattern_{false};
    std::string subscribeCommand_;
    std::string unsubscribeCommand_;
    std::mutex mutex_;
    std::list<RedisMessageCallback> messageCallbacks_;
    bool disabled_{false};
};

}  // namespace drogon::nosql
