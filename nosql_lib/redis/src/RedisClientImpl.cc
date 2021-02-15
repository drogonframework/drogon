/**
 *
 *  @file RedisClientImpl.cc
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

#include "RedisClientImpl.h"
using namespace drogon::nosql;
std::shared_ptr<RedisClient> RedisClient::newRedisClient(
    const trantor::InetAddress &serverAddress,
    const size_t connectionNumber,
    const std::string &password)
{
    return std::make_shared<RedisClientImpl>(serverAddress,
                                             connectionNumber,
                                             password);
}

RedisClientImpl::RedisClientImpl(const trantor::InetAddress &serverAddress,
                                 const size_t connectionsNumber,
                                 const std::string &password)
    : loops_(connectionsNumber < std::thread::hardware_concurrency()
                 ? connectionsNumber
                 : std::thread::hardware_concurrency(),
             "RedisLoop"),
      serverAddr_(serverAddress),
      password_(password),
      connectionsNumber_(connectionsNumber)
{
    for (size_t i = 0; i < connectionsNumber_; ++i)
    {
        auto loop = loops_.getNextLoop();
        loop->runInLoop([this, loop]() {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_.insert(newConnection(loop));
        });
    }
    loops_.start();
}

std::shared_ptr<RedisConnection> RedisClientImpl::newConnection(
    trantor::EventLoop *loop)
{
    return std::make_shared<RedisConnection>(serverAddr_, password_, loop);
}

void RedisClientImpl::execCommandAsync(
    const std::string &command,
    std::function<void(const RedisResult &)> &&commandCallback,
    std::function<void(const std::exception &)> &&exceptCallback) noexcept
{
    (*connections_.begin())
         ->sendCommand(command,
                       std::move(commandCallback),
                       std::move(exceptCallback));
}