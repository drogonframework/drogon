/**
 *
 *  @file RedisSubscriber.h
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

#include <string>
#include <drogon/nosql/RedisResult.h>

namespace drogon::nosql
{
class RedisSubscriber
{
  public:
    /**
     * @brief Subscribe to a channel. The subscriber will keep the channel
     * subscribed, until unsubscribe() is called on this channel, or the
     * subscriber or RedisClient who creates it no longer exists.
     * This method will not block.
     *
     * @param messageCallback The callback is called when a message is received
     * from the channel.
     * @param channel The channel to subscribe to.
     *
     * @note: Subscribing to same channel multiple times is supported. All
     * message callbacks will be called in subscription order.
     * One unsubscribe() call will remove all callbacks.
     */
    virtual void subscribe(const std::string &channel,
                           RedisMessageCallback &&messageCallback) noexcept = 0;

    // Subscribe to channel pattern
    virtual void psubscribe(
        const std::string &pattern,
        RedisMessageCallback &&messageCallback) noexcept = 0;

    /**
     * @brief Unsubscribe from a channel. Once this function returns, the
     * messageCallback registered through subscribe() will no longer be called.
     * This method will not block.
     *
     * @param channel The channel to subscribe to.
     */
    virtual void unsubscribe(const std::string &channel) noexcept = 0;

    // Unsubscribe from channel pattern
    virtual void punsubscribe(const std::string &pattern) noexcept = 0;

    virtual ~RedisSubscriber() = default;
};

}  // namespace drogon::nosql
