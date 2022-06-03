/**
 *
 *  RedisClientManagerSkipped.cc
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

#include "RedisClientManager.h"
#include <drogon/config.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include <cstdlib>

using namespace drogon::nosql;
using namespace drogon;

void RedisClientManager::createRedisClients(
    const std::vector<trantor::EventLoop *> & /*ioloops*/)
{
    return;
}

void RedisClientManager::createRedisClient(const std::string & /*name*/,
                                           const std::string & /*host*/,
                                           unsigned short /*port*/,
                                           const std::string & /*username*/,
                                           const std::string & /*password*/,
                                           size_t /*connectionNum*/,
                                           bool /*isFast*/,
                                           double /*timeout*/,
                                           unsigned int /*db*/)
{
    LOG_FATAL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

// bool RedisClientManager::areAllRedisClientsAvailable() const noexcept
// {
//     LOG_FATAL << "Redis is supported by drogon, please install the "
//                  "hiredis library first.";
//     abort();
// }

RedisClientManager::~RedisClientManager()
{
}