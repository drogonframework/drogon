/**
 *
 *  @file RedisClient.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <drogon/nosql/RedisResult.h>
#include <trantor/net/InetAddress.h>
#include <memory>
#include <functional>

namespace drogon
{
namespace nosql
{
class RedisClient
{
  public:
    static std::shared_ptr<RedisClient> newRedisClient(
        const trantor::InetAddress &serverAddress,
        const size_t connectionNumber = 1,
        const std::string &password = "");
    virtual void execCommandAsync(
        const std::string &command,
        std::function<void(const RedisResult &)> &&commandCallback,
        std::function<void(const std::exception &)>
            &&exceptCallback) noexcept = 0;
    virtual ~RedisClient() = default;
};
}  // namespace nosql
}  // namespace drogon