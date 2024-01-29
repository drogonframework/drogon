/**
 *
 *  RedisClientSkipped.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "drogon/nosql/RedisClient.h"

namespace drogon
{
namespace nosql
{
std::shared_ptr<RedisClient> RedisClient::newRedisClient(
    const trantor::InetAddress & /*serverAddress*/,
    size_t /*numberOfConnections*/,
    const std::string & /*password*/,
    const unsigned int /*db*/,
    const std::string & /*username*/)
{
    LOG_FATAL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}
}  // namespace nosql
}  // namespace drogon
