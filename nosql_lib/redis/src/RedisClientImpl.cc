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
#include "RedisTransactionImpl.h"
using namespace drogon::nosql;
std::shared_ptr<RedisClient> RedisClient::newRedisClient(
    const trantor::InetAddress &serverAddress,
    size_t connectionNumber,
    const std::string &password)
{
    return std::make_shared<RedisClientImpl>(serverAddress,
                                             connectionNumber,
                                             password);
}

RedisClientImpl::RedisClientImpl(const trantor::InetAddress &serverAddress,
                                 size_t numberOfConnections,
                                 std::string password)
    : loops_(numberOfConnections < std::thread::hardware_concurrency()
                 ? numberOfConnections
                 : std::thread::hardware_concurrency(),
             "RedisLoop"),
      serverAddr_(serverAddress),
      password_(std::move(password)),
      numberOfConnections_(numberOfConnections)
{
    loops_.start();
    for (size_t i = 0; i < numberOfConnections_; ++i)
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
    conn->setConnectCallback([thisWeakPtr](RedisConnectionPtr &&conn) {
        auto thisPtr = thisWeakPtr.lock();
        if (thisPtr)
        {
            {
                std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
                thisPtr->readyConnections_.push_back(conn);
            }
            thisPtr->handleNextTask(conn);
        }
    });
    conn->setDisconnectCallback([thisWeakPtr](RedisConnectionPtr &&conn) {
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
    conn->setIdleCallback([thisWeakPtr](const RedisConnectionPtr &connPtr) {
        auto thisPtr = thisWeakPtr.lock();
        if (thisPtr)
        {
            thisPtr->handleNextTask(connPtr);
        }
    });
    return conn;
}

void RedisClientImpl::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    string_view command,
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
        va_start(args, command);
        connPtr->sendvCommand(command,
                              std::move(resultCallback),
                              std::move(exceptionCallback),
                              args);
        va_end(args);
    }
    else
    {
        LOG_TRACE << "no connection available, push command to buffer";
        std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
        va_list args;
        va_start(args, command);
        auto formattedCmd = RedisConnection::getFormattedCommand(command, args);
        va_end(args);
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        tasks_.emplace([thisWeakPtr,
                        resultCallback = std::move(resultCallback),
                        exceptionCallback = std::move(exceptionCallback),
                        formattedCmd = std::move(formattedCmd)](
                           const RedisConnectionPtr &connPtr) mutable {
            connPtr->sendFormattedCommand(std::move(formattedCmd),
                                          std::move(resultCallback),
                                          std::move(exceptionCallback));
        });
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

void RedisClientImpl::newTransactionAsync(
    const std::function<void(const std::shared_ptr<RedisTransaction> &)>
        &callback)
{
    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (!readyConnections_.empty())
        {
            connPtr = readyConnections_[readyConnections_.size() - 1];
            readyConnections_.resize(readyConnections_.size() - 1);
        }
    }
    if (connPtr)
    {
        callback(makeTransaction(connPtr));
    }
    else
    {
        std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        tasks_.emplace(
            [callback, thisWeakPtr](const RedisConnectionPtr &connPtr) {
                auto thisPtr = thisWeakPtr.lock();
                if (thisPtr)
                {
                    thisPtr->newTransactionAsync(callback);
                }
            });
    }
}

std::shared_ptr<RedisTransaction> RedisClientImpl::makeTransaction(
    const RedisConnectionPtr &connPtr)
{
    std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
    auto trans = std::shared_ptr<RedisTransactionImpl>(
        new RedisTransactionImpl(connPtr),
        [thisWeakPtr, connPtr](RedisTransactionImpl *p) {
            auto thisPtr = thisWeakPtr.lock();
            if (thisPtr)
            {
                {
                    std::lock_guard<std::mutex> lock(
                        thisPtr->connectionsMutex_);
                    thisPtr->readyConnections_.push_back(connPtr);
                }
                thisPtr->handleNextTask(connPtr);
            }
        });
    trans->doBegin();
    return trans;
}

void RedisClientImpl::handleNextTask(const RedisConnectionPtr &connPtr)
{
    std::function<void(const RedisConnectionPtr &)> task;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (!tasks_.empty())
        {
            task = std::move(tasks_.front());
            tasks_.pop();
        }
    }
    if (task)
    {
        task(connPtr);
    }
}