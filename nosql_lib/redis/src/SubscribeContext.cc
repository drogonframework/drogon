/**
 *
 *  @file SubscribeContext.cc
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
#include "SubscribeContext.h"
#include "RedisSubscriberImpl.h"
#include <stdio.h>
#include <string>
#include <utility>

using namespace drogon::nosql;

std::atomic<unsigned long long> SubscribeContext::maxContextId_{0};

static std::string formatSubscribeCommand(const std::string &channel,
                                          bool subscribe)
{
    // Avoid using redisvFormatCommand, we don't want to emit unknown error
    static const char *redisSubFmt =
        "*2\r\n$9\r\nsubscribe\r\n$%zu\r\n%.*s\r\n";
    static const char *redisUnsubFmt =
        "*2\r\n$11\r\nunsubscribe\r\n$%zu\r\n%.*s\r\n";

    const char *fmt = subscribe ? redisSubFmt : redisUnsubFmt;

    std::string command;
    if (channel.size() < 32)
    {
        char buf[64];
        size_t bufSize = sizeof(buf);
        int len = snprintf(buf,
                           bufSize,
                           fmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
    }
    else
    {
        size_t bufSize = channel.size() + 64;
        char *buf = static_cast<char *>(malloc(bufSize));
        int len = snprintf(buf,
                           bufSize,
                           fmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
        free(buf);
    }
    return command;
}

SubscribeContext::SubscribeContext(std::weak_ptr<RedisSubscriber> &&weakSub,
                                   const std::string &channel)
    : contextId_(++maxContextId_),
      weakSub_(std::move(weakSub)),
      channel_(channel),
      subscribeCommand_(formatSubscribeCommand(channel, true)),
      unsubscribeCommand_(formatSubscribeCommand(channel, false))
{
}

void SubscribeContext::onMessage(const std::string &channel,
                                 const std::string &message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &callback : messageCallbacks_)
    {
        callback(channel, message);
    }
}

void SubscribeContext::onSubscribe()
{
}

void SubscribeContext::onUnsubscribe()
{
}
