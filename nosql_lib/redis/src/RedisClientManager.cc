/**
 *
 *  @file RedisClientManager.cc
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

#include "../../lib/src/RedisClientManager.h"
#include "RedisClientLockFree.h"
#include "RedisClientImpl.h"

#include <algorithm>

using namespace drogon::nosql;
using namespace drogon;

void RedisClientManager::createRedisClients(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    assert(redisClientsMap_.empty());
    assert(redisFastClientsMap_.empty());
    for (auto &redisInfo : redisInfos_)
    {
        if (redisInfo.isFast_)
        {
            redisFastClientsMap_[redisInfo.name_] =
                IOThreadStorage<RedisClientPtr>();
            redisFastClientsMap_[redisInfo.name_].init([&](RedisClientPtr &c,
                                                           size_t idx) {
                assert(idx == ioLoops[idx]->index());
                LOG_TRACE << "create fast redis client for the thread " << idx;
                c = std::make_shared<RedisClientLockFree>(
                    trantor::InetAddress(redisInfo.addr_, redisInfo.port_),
                    redisInfo.connectionNumber_,
                    ioLoops[idx],
                    redisInfo.username_,
                    redisInfo.password_,
                    redisInfo.db_);
                if (redisInfo.timeout_ > 0.0)
                {
                    c->setTimeout(redisInfo.timeout_);
                }
            });
        }
        else
        {
            auto clientPtr = std::make_shared<RedisClientImpl>(
                trantor::InetAddress(redisInfo.addr_, redisInfo.port_),
                redisInfo.connectionNumber_,
                redisInfo.username_,
                redisInfo.password_,
                redisInfo.db_);
            if (redisInfo.timeout_ > 0.0)
            {
                clientPtr->setTimeout(redisInfo.timeout_);
            }
            clientPtr->init();
            redisClientsMap_[redisInfo.name_] = std::move(clientPtr);
        }
    }
}

void RedisClientManager::createRedisClient(const std::string &name,
                                           const std::string &addr,
                                           unsigned short port,
                                           const std::string &username,
                                           const std::string &password,
                                           const size_t connectionNum,
                                           const bool isFast,
                                           double timeout,
                                           unsigned int db)
{
    RedisInfo info;
    info.name_ = name;
    info.addr_ = addr;
    info.port_ = port;
    info.username_ = username;
    info.password_ = password;
    info.connectionNumber_ = connectionNum;
    info.isFast_ = isFast;
    info.timeout_ = timeout;
    info.db_ = db;

    redisInfos_.emplace_back(std::move(info));
}

// bool RedisClientManager::areAllRedisClientsAvailable() const noexcept
//{
//    for (auto const &pair : redisClientsMap_)
//    {
//        if (!(pair.second)->hasAvailableConnections())
//            return false;
//    }
//    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
//    if (loop && loop->index() < app().getThreadNum())
//    {
//        for (auto const &pair : redisFastClientsMap_)
//        {
//            if (!(*(pair.second))->hasAvailableConnections())
//                return false;
//        }
//    }
//    return true;
//}

RedisClientManager::~RedisClientManager()
{
    for (auto &pair : redisClientsMap_)
    {
        pair.second->closeAll();
    }
    for (auto &pair : redisFastClientsMap_)
    {
        pair.second.init([](RedisClientPtr &clientPtr, size_t index) {
            // the main loop;
            std::promise<void> p;
            auto f = p.get_future();
            drogon::getIOThreadStorageLoop(index)->runInLoop(
                [&clientPtr, &p]() {
                    clientPtr->closeAll();
                    p.set_value();
                });
            f.get();
        });
    }
}
