/**
 *
 *  @file RedisSubscriberImpl.h
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

#include <drogon/nosql/RedisSubscriber.h>
#include "RedisConnection.h"
#include "SubscribeContext.h"

#include <mutex>
#include <unordered_map>
#include <memory>
#include <list>

namespace drogon::nosql
{
class RedisSubscriberImpl
    : public RedisSubscriber,
      public std::enable_shared_from_this<RedisSubscriberImpl>
{
  public:
    ~RedisSubscriberImpl() override;

    void subscribe(RedisMessageCallback &&messageCallback,
                   const std::string &channel) noexcept override;

    void unsubscribe(const std::string &channel) noexcept override;

    // Set a connected connection to subscriber.
    void setConnection(const RedisConnectionPtr &conn);
    // Clear connection and task queue.
    void clearConnection();
    // Subscribe next channel in task queue.
    void subscribeNext();
    // Subscribe all channels.
    void subscribeAll();
    // Remove unsub-ed channel context
    void removeContext(const std::string &channel);

  private:
    RedisConnectionPtr conn_;
    std::unordered_map<std::string, std::shared_ptr<SubscribeContext>>
        subscribeContexts_;
    std::unordered_map<std::string, std::shared_ptr<SubscribeContext>>
        unsubContexts_;
    std::list<std::shared_ptr<std::function<void(const RedisConnectionPtr &)>>>
        tasks_;
    std::mutex mutex_;
};

}  // namespace drogon::nosql
