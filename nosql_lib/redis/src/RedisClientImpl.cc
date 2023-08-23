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

#include "RedisConnection.h"
#include "RedisClientImpl.h"
#include "RedisSubscriberImpl.h"
#include "RedisTransactionImpl.h"
#include "../../lib/src/TaskTimeoutFlag.h"

using namespace drogon::nosql;

std::shared_ptr<RedisClient> RedisClient::newRedisClient(
    const trantor::InetAddress &serverAddress,
    size_t connectionNumber,
    const std::string &password,
    unsigned int db,
    const std::string &username)
{
    auto client = std::make_shared<RedisClientImpl>(
        serverAddress, connectionNumber, username, password, db);
    client->init();
    return client;
}

RedisClientImpl::RedisClientImpl(const trantor::InetAddress &serverAddress,
                                 size_t numberOfConnections,
                                 std::string username,
                                 std::string password,
                                 unsigned int db)
    : loops_(numberOfConnections < std::thread::hardware_concurrency()
                 ? numberOfConnections
                 : std::thread::hardware_concurrency(),
             "RedisLoop"),
      serverAddr_(serverAddress),
      username_(std::move(username)),
      password_(std::move(password)),
      db_(db),
      numberOfConnections_(numberOfConnections)
{
}

void RedisClientImpl::init()
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
    auto conn = std::make_shared<RedisConnection>(
        serverAddr_, username_, password_, db_, loop);
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
            loop->runAfter(2.0, [thisPtr, loop, conn]() {
                std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
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

RedisConnectionPtr RedisClientImpl::newSubscribeConnection(
    trantor::EventLoop *loop,
    const std::shared_ptr<RedisSubscriberImpl> &subscriber)
{
    auto conn = std::make_shared<RedisConnection>(
        serverAddr_, username_, password_, db_, loop);
    std::weak_ptr<RedisClientImpl> weakThis = shared_from_this();
    std::weak_ptr<RedisSubscriberImpl> weakSub(subscriber);
    conn->setConnectCallback([weakThis, weakSub](RedisConnectionPtr &&conn) {
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
            return;
        auto subPtr = weakSub.lock();

        std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
        if (subPtr)
        {
            subPtr->setConnection(conn);
            subPtr->subscribeAll();
        }
        else
        {
            thisPtr->connections_.erase(conn);
        }
    });
    conn->setDisconnectCallback([weakThis, weakSub](RedisConnectionPtr &&conn) {
        // assert(status == REDIS_CONNECTED);
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
            return;
        std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
        thisPtr->connections_.erase(conn);
        auto subPtr = weakSub.lock();
        if (!subPtr)
            return;
        subPtr->clearConnection();

        auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
        assert(loop);
        loop->runAfter(2.0, [thisPtr, loop, subPtr]() {
            std::lock_guard<std::mutex> lock(thisPtr->connectionsMutex_);
            thisPtr->connections_.insert(
                thisPtr->newSubscribeConnection(loop, subPtr));
        });
    });
    conn->setIdleCallback([weakThis, weakSub](const RedisConnectionPtr &) {
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
            return;
        auto subPtr = weakSub.lock();
        if (!subPtr)
            return;
        subPtr->subscribeNext();
    });
    return conn;
}

void RedisClientImpl::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    std::string_view command,
    ...) noexcept
{
    if (timeout_ > 0.0)
    {
        va_list args;
        va_start(args, command);
        execCommandAsyncWithTimeout(command,
                                    std::move(resultCallback),
                                    std::move(exceptionCallback),
                                    args);
        va_end(args);
        return;
    }
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
        va_list args;
        va_start(args, command);
        auto formattedCmd = RedisConnection::getFormattedCommand(command, args);
        va_end(args);
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        tasks_.emplace_back(
            std::make_shared<std::function<void(const RedisConnectionPtr &)>>(
                [resultCallback = std::move(resultCallback),
                 exceptionCallback = std::move(exceptionCallback),
                 formattedCmd = std::move(formattedCmd)](
                    const RedisConnectionPtr &connPtr) mutable {
                    connPtr->sendFormattedCommand(std::move(formattedCmd),
                                                  std::move(resultCallback),
                                                  std::move(exceptionCallback));
                }));
    }
}

RedisClientImpl::~RedisClientImpl()
{
    closeAll();
}

void RedisClientImpl::closeAll()
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
        if (timeout_ <= 0.0)
        {
            std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            tasks_.emplace_back(
                std::make_shared<
                    std::function<void(const RedisConnectionPtr &)>>(
                    [callback,
                     thisWeakPtr](const RedisConnectionPtr & /*connPtr*/) {
                        auto thisPtr = thisWeakPtr.lock();
                        if (thisPtr)
                        {
                            thisPtr->newTransactionAsync(callback);
                        }
                    }));
        }
        else
        {
            auto callbackPtr = std::make_shared<
                std::function<void(const std::shared_ptr<RedisTransaction> &)>>(
                callback);
            auto transCbPtr = std::make_shared<std::weak_ptr<
                std::function<void(const RedisConnectionPtr &)>>>();
            auto timeoutFlagPtr = std::make_shared<TaskTimeoutFlag>(
                loops_.getNextLoop(),
                std::chrono::duration<double>(timeout_),
                [callbackPtr, transCbPtr, this]() {
                    auto cbPtr = (*transCbPtr).lock();
                    if (cbPtr)
                    {
                        std::lock_guard<std::mutex> lock(connectionsMutex_);
                        for (auto iter = tasks_.begin(); iter != tasks_.end();
                             ++iter)
                        {
                            if (cbPtr == *iter)
                            {
                                tasks_.erase(iter);
                                break;
                            }
                        }
                    }
                    (*callbackPtr)(nullptr);
                });
            std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
            auto bufferCbPtr = std::make_shared<
                std::function<void(const RedisConnectionPtr &)>>(
                [callbackPtr, timeoutFlagPtr, thisWeakPtr](
                    const RedisConnectionPtr & /*connPtr*/) {
                    auto thisPtr = thisWeakPtr.lock();
                    if (thisPtr)
                    {
                        if (timeoutFlagPtr->done())
                        {
                            return;
                        }
                        thisPtr->newTransactionAsync(*callbackPtr);
                    }
                });
            {
                std::lock_guard<std::mutex> lock(connectionsMutex_);
                tasks_.emplace_back(bufferCbPtr);
            }
            (*transCbPtr) = bufferCbPtr;
            timeoutFlagPtr->runTimer();
        }
    }
}

std::shared_ptr<RedisTransaction> RedisClientImpl::makeTransaction(
    const RedisConnectionPtr &connPtr)
{
    std::weak_ptr<RedisClientImpl> thisWeakPtr = shared_from_this();
    auto trans = std::shared_ptr<RedisTransactionImpl>(
        new RedisTransactionImpl(connPtr),
        [thisWeakPtr, connPtr](RedisTransactionImpl *p) {
            delete p;
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
    std::shared_ptr<std::function<void(const RedisConnectionPtr &)>> taskPtr;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (!tasks_.empty())
        {
            taskPtr = std::move(tasks_.front());
            tasks_.pop_front();
        }
    }
    if (taskPtr && (*taskPtr))
    {
        (*taskPtr)(connPtr);
    }
}

void RedisClientImpl::execCommandAsyncWithTimeout(
    std::string_view command,
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    va_list ap)
{
    auto expCbPtr =
        std::make_shared<RedisExceptionCallback>(std::move(exceptionCallback));
    auto bufferCbPtr = std::make_shared<
        std::weak_ptr<std::function<void(const RedisConnectionPtr &)>>>();
    auto timeoutFlagPtr = std::make_shared<TaskTimeoutFlag>(
        loops_.getNextLoop(),
        std::chrono::duration<double>(timeout_),
        [expCbPtr, bufferCbPtr, this]() {
            auto bfCbPtr = (*bufferCbPtr).lock();
            if (bfCbPtr)
            {
                std::lock_guard<std::mutex> lock(connectionsMutex_);
                for (auto iter = tasks_.begin(); iter != tasks_.end(); ++iter)
                {
                    if (bfCbPtr == *iter)
                    {
                        tasks_.erase(iter);
                        break;
                    }
                }
            }
            if (*expCbPtr)
            {
                (*expCbPtr)(RedisException(RedisErrorCode::kTimeout,
                                           "Command execution timeout"));
            }
        });
    auto newResultCallback = [resultCallback = std::move(resultCallback),
                              timeoutFlagPtr](const RedisResult &result) {
        if (timeoutFlagPtr->done())
        {
            return;
        }
        if (resultCallback)
        {
            resultCallback(result);
        }
    };
    auto newExceptionCallback = [expCbPtr,
                                 timeoutFlagPtr](const RedisException &err) {
        if (timeoutFlagPtr->done())
        {
            return;
        }
        if (*expCbPtr)
        {
            (*expCbPtr)(err);
        }
    };
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
        connPtr->sendvCommand(command,
                              std::move(newResultCallback),
                              std::move(newExceptionCallback),
                              ap);
    }
    else
    {
        LOG_TRACE << "no connection available, push command to buffer";
        auto formattedCmd = RedisConnection::getFormattedCommand(command, ap);
        auto bfCbPtr =
            std::make_shared<std::function<void(const RedisConnectionPtr &)>>(
                [resultCallback = std::move(newResultCallback),
                 exceptionCallback = std::move(newExceptionCallback),
                 formattedCmd = std::move(formattedCmd)](
                    const RedisConnectionPtr &connPtr) mutable {
                    connPtr->sendFormattedCommand(std::move(formattedCmd),
                                                  std::move(resultCallback),
                                                  std::move(exceptionCallback));
                });
        (*bufferCbPtr) = bfCbPtr;
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        tasks_.emplace_back(bfCbPtr);
    }
    timeoutFlagPtr->runTimer();
}

std::shared_ptr<RedisSubscriber> RedisClientImpl::newSubscriber() noexcept
{
    auto subscriber = std::make_shared<RedisSubscriberImpl>();
    auto loop = loops_.getNextLoop();
    loop->queueInLoop([this, loop, subscriber]() {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.insert(newSubscribeConnection(loop, subscriber));
    });

    return subscriber;
}
