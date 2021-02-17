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
#include <drogon/utils/string_view.h>
#include <trantor/net/InetAddress.h>
#include <memory>
#include <functional>

namespace drogon
{
namespace nosql
{
/**
 * @brief This class represents a redis client that comtains several connections
 * to a redis server.
 *
 */
class RedisClient
{
  public:
    /**
     * @brief Create a new redis client with multiple connections;
     *
     * @param serverAddress The server address.
     * @param numberOfConnections The number of connections. 1 by default.
     * @param password The password to authenticate if necessary.
     * @return std::shared_ptr<RedisClient>
     */
    static std::shared_ptr<RedisClient> newRedisClient(
        const trantor::InetAddress &serverAddress,
        const size_t numberOfConnections = 1,
        const std::string &password = "");
    /**
     * @brief Execute a redis command
     *
     * @param commandCallback The callback is called when a redis reply is
     * received successfully.
     * @param exceptionCallback The callback is called when an error occurs.
     * @note When a redis reply with REDIS_REPLY_ERROR code is received, this
     * callback is called.
     * @param command The command to be executed. the command string can contain
     * some placeholders for parameters, such as '%s', '%d', etc.
     * @param ... The command parameters.
     * For example:
     * @code
       redisClientPtr->execCommandAsync([](const RedisResult &r){
           std::cout << r.getStringForDisplaying() << std::endl;
       },[](const std::exception &err){
           std::cerr << err.what() << std::endl;
       }, "get %s", key.data());
       @endcode
     */
    virtual void execCommandAsync(
        std::function<void(const RedisResult &)> &&commandCallback,
        std::function<void(const std::exception &)> &&exceptionCallback,
        string_view command,
        ...) noexcept = 0;
    virtual ~RedisClient() = default;
};
}  // namespace nosql
}  // namespace drogon