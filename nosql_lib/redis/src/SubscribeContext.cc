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
#include <stdio.h>
#include <string>
#include <utility>

using namespace drogon::nosql;

std::atomic<unsigned long long> SubscribeContext::maxContextId_{0};

enum class SubCommandType
{
    Subscribe,
    Unsubscribe,
    Psubscribe,
    Punsubscribe
};

static std::string formatSubscribeCommand(const std::string &channel,
                                          SubCommandType type)
{
    // Avoid using redisvFormatCommand, we don't want to emit unknown error
    static const char *redisSubFmt =
        "*2\r\n$9\r\nsubscribe\r\n$%zu\r\n%.*s\r\n";
    static const char *redisUnsubFmt =
        "*2\r\n$11\r\nunsubscribe\r\n$%zu\r\n%.*s\r\n";
    static const char *redisPsubFmt =
        "*2\r\n$10\r\npsubscribe\r\n$%zu\r\n%.*s\r\n";
    static const char *redisPunsubFmt =
        "*2\r\n$12\r\npunsubscribe\r\n$%zu\r\n%.*s\r\n";

    const char *fmt;
    switch (type)
    {
        case SubCommandType::Subscribe:
            fmt = redisSubFmt;
            break;
        case SubCommandType::Unsubscribe:
            fmt = redisUnsubFmt;
            break;
        case SubCommandType::Psubscribe:
            fmt = redisPsubFmt;
            break;
        case SubCommandType::Punsubscribe:
            fmt = redisPunsubFmt;
            break;
    }

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
                                   const std::string &channel,
                                   bool isPattern)
    : contextId_(++maxContextId_),
      weakSub_(std::move(weakSub)),
      channel_(channel),
      isPattern_(isPattern)
{
    if (isPattern)
    {
        subscribeCommand_ =
            formatSubscribeCommand(channel, SubCommandType::Psubscribe);
        unsubscribeCommand_ =
            formatSubscribeCommand(channel, SubCommandType::Punsubscribe);
    }
    else
    {
        subscribeCommand_ =
            formatSubscribeCommand(channel, SubCommandType::Subscribe);
        unsubscribeCommand_ =
            formatSubscribeCommand(channel, SubCommandType::Unsubscribe);
    }
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

void SubscribeContext::onSubscribe(const std::string &channel,
                                   long long numChannels)
{
    LOG_DEBUG << "Subscribe success to [" << channel << "], total "
              << numChannels;
}

void SubscribeContext::onUnsubscribe(const std::string &channel,
                                     long long numChannels)
{
    LOG_DEBUG << "Unsubscribe success from [" << channel << "], total "
              << numChannels;
}
