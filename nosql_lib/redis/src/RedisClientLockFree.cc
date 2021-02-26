/**
 *
 *  @file RedisClientLockFree.cc
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

#include "RedisClientLockFree.h"
#include "RedisTransactionImpl.h"
using namespace drogon::nosql;

RedisClientLockFree::RedisClientLockFree(
    const trantor::InetAddress &serverAddress,
    size_t numberOfConnections,
    trantor::EventLoop *loop,
    std::string password)
    : loop_(loop),
      serverAddr_(serverAddress),
      password_(std::move(password)),
      numberOfConnections_(numberOfConnections)
{
    assert(loop_);
    for (size_t i = 0; i < numberOfConnections_; ++i)
    {
        loop_->queueInLoop([this]() { connections_.insert(newConnection()); });
    }
}

RedisConnectionPtr RedisClientLockFree::newConnection()
{
    loop_->assertInLoopThread();
    auto conn =
        std::make_shared<RedisConnection>(serverAddr_, password_, loop_);
    std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
    conn->setConnectCallback([thisWeakPtr](RedisConnectionPtr &&conn) {
        auto thisPtr = thisWeakPtr.lock();
        if (thisPtr)
        {
            thisPtr->readyConnections_.push_back(conn);
            thisPtr->handleNextTask(conn);
        }
    });
    conn->setDisconnectCallback([thisWeakPtr](RedisConnectionPtr &&conn) {
        // assert(status == REDIS_CONNECTED);
        auto thisPtr = thisWeakPtr.lock();
        if (thisPtr)
        {
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
            thisPtr->loop_->runAfter(2.0, [thisPtr]() {
                thisPtr->connections_.insert(thisPtr->newConnection());
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

void RedisClientLockFree::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    string_view command,
    ...) noexcept
{
    loop_->assertInLoopThread();
    RedisConnectionPtr connPtr;
    {
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
        std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
        va_list args;
        va_start(args, command);
        auto formattedCmd = RedisConnection::getFormattedCommand(command, args);
        va_end(args);
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

RedisClientLockFree::~RedisClientLockFree()
{
    for (auto &conn : connections_)
    {
        conn->disconnect();
    }
    readyConnections_.clear();
    connections_.clear();
}

void RedisClientLockFree::newTransactionAsync(
    const std::function<void(const std::shared_ptr<RedisTransaction> &)>
        &callback)
{
    loop_->assertInLoopThread();
    RedisConnectionPtr connPtr;

    if (!readyConnections_.empty())
    {
        connPtr = readyConnections_[readyConnections_.size() - 1];
        readyConnections_.resize(readyConnections_.size() - 1);
    }

    if (connPtr)
    {
        callback(makeTransaction(connPtr));
    }
    else
    {
        std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
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

std::shared_ptr<RedisTransaction> RedisClientLockFree::makeTransaction(
    const RedisConnectionPtr &connPtr)
{
    std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
    auto trans = std::shared_ptr<RedisTransactionImpl>(
        new RedisTransactionImpl(connPtr),
        [thisWeakPtr, connPtr](RedisTransactionImpl *p) {
            auto thisPtr = thisWeakPtr.lock();
            if (thisPtr)
            {
                thisPtr->readyConnections_.push_back(connPtr);
                thisPtr->handleNextTask(connPtr);
            }
        });
    trans->doBegin();
    return trans;
}

void RedisClientLockFree::handleNextTask(const RedisConnectionPtr &connPtr)
{
    loop_->assertInLoopThread();
    std::function<void(const RedisConnectionPtr &)> task;

    if (!tasks_.empty())
    {
        task = std::move(tasks_.front());
        tasks_.pop();
    }

    if (task)
    {
        task(connPtr);
    }
}