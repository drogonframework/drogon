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

#include "RedisConnection.h"
#include "RedisClientLockFree.h"
#include "RedisSubscriberImpl.h"
#include "RedisTransactionImpl.h"
#include "../../lib/src/TaskTimeoutFlag.h"
using namespace drogon::nosql;

RedisClientLockFree::RedisClientLockFree(
    const trantor::InetAddress &serverAddress,
    size_t numberOfConnections,
    trantor::EventLoop *loop,
    std::string username,
    std::string password,
    unsigned int db)
    : loop_(loop),
      serverAddr_(serverAddress),
      username_(std::move(username)),
      password_(std::move(password)),
      db_(db),
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
    auto conn = std::make_shared<RedisConnection>(
        serverAddr_, username_, password_, db_, loop_);
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
            thisPtr->loop_->runAfter(2.0, [thisPtr, conn]() {
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

RedisConnectionPtr RedisClientLockFree::newSubscribeConnection(
    const std::shared_ptr<RedisSubscriberImpl> &subscriber)
{
    loop_->assertInLoopThread();
    auto conn = std::make_shared<RedisConnection>(
        serverAddr_, username_, password_, db_, loop_);
    std::weak_ptr<RedisClientLockFree> weakThis = shared_from_this();
    std::weak_ptr<RedisSubscriberImpl> weakSub(subscriber);
    conn->setConnectCallback([weakThis, weakSub](RedisConnectionPtr &&conn) {
        conn->getLoop()->assertInLoopThread();  // TODO: remove
        auto thisPtr = weakThis.lock();
        if (!thisPtr)
            return;
        thisPtr->loop_->assertInLoopThread();  // TODO: remove
        auto subPtr = weakSub.lock();
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
        thisPtr->connections_.erase(conn);
        auto subPtr = weakSub.lock();
        if (!subPtr)
            return;
        subPtr->clearConnection();

        thisPtr->loop_->runAfter(2.0, [thisPtr, subPtr]() {
            thisPtr->connections_.insert(
                thisPtr->newSubscribeConnection(subPtr));
        });
    });
    conn->setIdleCallback(
        [weakThis, weakSub](const RedisConnectionPtr &connPtr) {
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

void RedisClientLockFree::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    string_view command,
    ...) noexcept
{
    loop_->assertInLoopThread();
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
        tasks_.emplace_back(
            std::make_shared<std::function<void(const RedisConnectionPtr &)>>(
                [thisWeakPtr,
                 resultCallback = std::move(resultCallback),
                 exceptionCallback = std::move(exceptionCallback),
                 formattedCmd = std::move(formattedCmd)](
                    const RedisConnectionPtr &connPtr) mutable {
                    connPtr->sendFormattedCommand(std::move(formattedCmd),
                                                  std::move(resultCallback),
                                                  std::move(exceptionCallback));
                }));
    }
}

RedisClientLockFree::~RedisClientLockFree()
{
    closeAll();
}

void RedisClientLockFree::closeAll()
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
        if (timeout_ <= 0.0)
        {
            std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
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
                loop_,
                std::chrono::duration<double>(timeout_),
                [callbackPtr, transCbPtr, this]() {
                    auto cbPtr = (*transCbPtr).lock();
                    if (cbPtr)
                    {
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
            std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
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
            tasks_.emplace_back(bufferCbPtr);

            (*transCbPtr) = bufferCbPtr;
            timeoutFlagPtr->runTimer();
        }
    }
}

std::shared_ptr<RedisTransaction> RedisClientLockFree::makeTransaction(
    const RedisConnectionPtr &connPtr)
{
    std::weak_ptr<RedisClientLockFree> thisWeakPtr = shared_from_this();
    auto trans = std::shared_ptr<RedisTransactionImpl>(
        new RedisTransactionImpl(connPtr),
        [thisWeakPtr, connPtr](RedisTransactionImpl *p) {
            delete p;
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
    std::shared_ptr<std::function<void(const RedisConnectionPtr &)>> taskPtr;

    if (!tasks_.empty())
    {
        taskPtr = std::move(tasks_.front());
        tasks_.pop_front();
    }

    if (taskPtr && (*taskPtr))
    {
        (*taskPtr)(connPtr);
    }
}

void RedisClientLockFree::execCommandAsyncWithTimeout(
    string_view command,
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    va_list ap)
{
    auto expCbPtr =
        std::make_shared<RedisExceptionCallback>(std::move(exceptionCallback));
    auto bufferCbPtr = std::make_shared<
        std::weak_ptr<std::function<void(const RedisConnectionPtr &)>>>();
    auto timeoutFlagPtr = std::make_shared<TaskTimeoutFlag>(
        loop_,
        std::chrono::duration<double>(timeout_),
        [expCbPtr, bufferCbPtr, this]() {
            auto bfCbPtr = (*bufferCbPtr).lock();
            if (bfCbPtr)
            {
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
        tasks_.emplace_back(bfCbPtr);
    }
    timeoutFlagPtr->runTimer();
}

std::shared_ptr<RedisSubscriber> RedisClientLockFree::newSubscriber() noexcept
{
    auto subscriber = std::make_shared<RedisSubscriberImpl>();
    loop_->runInLoop([this, subscriber]() {
        connections_.insert(newSubscribeConnection(subscriber));
    });

    return subscriber;
}
