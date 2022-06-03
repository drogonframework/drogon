/**
 *
 *  RedisClientManager.h
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

#pragma once

#include <drogon/nosql/RedisClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/IOThreadStorage.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <string>
#include <memory>

namespace drogon
{
namespace nosql
{
class RedisClientManager : public trantor::NonCopyable
{
  public:
    void createRedisClients(const std::vector<trantor::EventLoop *> &ioLoops);
    RedisClientPtr getRedisClient(const std::string &name)
    {
        assert(redisClientsMap_.find(name) != redisClientsMap_.end());
        return redisClientsMap_[name];
    }

    RedisClientPtr getFastRedisClient(const std::string &name)
    {
        auto iter = redisFastClientsMap_.find(name);
        assert(iter != redisFastClientsMap_.end());
        return iter->second.getThreadData();
    }
    void createRedisClient(const std::string &name,
                           const std::string &host,
                           unsigned short port,
                           const std::string &username,
                           const std::string &password,
                           size_t connectionNum,
                           bool isFast,
                           double timeout,
                           unsigned int db);
    // bool areAllRedisClientsAvailable() const noexcept;

    ~RedisClientManager();

  private:
    std::map<std::string, RedisClientPtr> redisClientsMap_;
    std::map<std::string, IOThreadStorage<RedisClientPtr>> redisFastClientsMap_;
    struct RedisInfo
    {
        std::string name_;
        std::string addr_;
        std::string username_;
        std::string password_;
        unsigned short port_;
        bool isFast_;
        size_t connectionNumber_;
        double timeout_;
        unsigned int db_;
    };
    std::vector<RedisInfo> redisInfos_;
};
}  // namespace nosql
}  // namespace drogon
