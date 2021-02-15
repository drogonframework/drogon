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

#include <trantor/net/InetAddress.h>
#include <memory>
namespace drogon
{
namespace nosql
{
class RedisClient
{
  public:
    static std::shared_ptr<RedisClient> newRedisClient(
        const trantor::InetAddress &serverAddress,
        const size_t connectionNumber,
        const std::string &password = "");
    virtual ~RedisClient()=default;
};
}  // namespace nosql
}  // namespace drogon