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
    loops_.start();
    for (size_t i = 0; i < connectionsNumber_; ++i)
    {
        auto loop = loops_.getNextLoop();
        loop->queueInLoop([this, loop]() {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_.insert(newConnection(loop));
        });
    }
}

RedisConnectionPtr RedisClientImpl::newConnection(trantor::EventLoop *loop)
{
    auto conn = std::make_shared<RedisConnection>(serverAddr_, password_, loop);
    std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
    conn->setConnectCallback(
        [thisWeakPtr](RedisConnectionPtr &&conn, int status) {
            assert(status == REDIS_OK);
            auto thisPtr = thisWeakPtr.lock();
            if (thisPtr)
            {
                std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
                thisPtr->readyConnections_.emplace_back(std::move(conn));
            }
        });
    conn->setDisconnectCallback(
        [thisWeakPtr](RedisConnectionPtr &&conn, int status) {
            // assert(status == REDIS_CONNECTED);
            auto thisPtr = thisWeakPtr.lock();
            if (thisPtr)
            {
                std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
                thisPtr->connections_.erase(conn);
                for (auto iter = thisPtr->readyConnections_.begin();
                     iter != thisPtr->readyConnections_.end();
                     ++iter)
                {
                    if (*iter == conn)
                    {
                        thisPtr->readyConnections_.erase(iter);
                        break;
                    }
                }
                auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
                assert(loop);
                loop->runAfter(2.0, [thisPtr, loop]() {
                    thisPtr->connections_.insert(thisPtr->newConnection(loop));
                });
            }
        });
    return conn;
}

void RedisClientImpl::execCommandAsync(
    const std::string &command,
    std::function<void(const RedisResult &)> &&commandCallback,
    std::function<void(const std::exception &)> &&exceptionCallback,
    ...) noexcept
{
    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (!readyConnections_.empty())
        {
            if (connectionPos_ >= readyConnections_.size())
            {
                connPtr = readyConnections_[0];
                connectionPos_ = 1;
            }
            else
            {
                connPtr = readyConnections_[connectionPos_++];
            }
        }
    }
    if (connPtr)
    {
        va_list args;
        va_start(args, exceptionCallback);
        connPtr->sendvCommand(command,
                              std::move(commandCallback),
                              std::move(exceptionCallback),
                              args);
        va_end(args);
    }
    else
    {
        exceptionCallback(std::runtime_error("no connection available!"));
    }
}

RedisClientImpl::~RedisClientImpl()
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto &conn : connections_)
    {
        conn->disconnect();
    }
    readyConnections_.clear();
    connections_.clear();
}