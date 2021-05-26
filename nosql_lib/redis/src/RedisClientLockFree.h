/**
 *
 *  @file RedisClientLockFree.h
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
#pragma once

#include "RedisConnection.h"
#include <drogon/nosql/RedisClient.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <vector>
#include <unordered_set>
#include <list>
#include <future>

namespace drogon
{
namespace nosql
{
class RedisClientLockFree final
    : public RedisClient,
      public trantor::NonCopyable,
      public std::enable_shared_from_this<RedisClientLockFree>
{
  public:
    RedisClientLockFree(const trantor::InetAddress &serverAddress,
                        size_t numberOfConnections,
                        trantor::EventLoop *loop,
                        std::string password = "",
                        unsigned int db = 0);
    void execCommandAsync(RedisResultCallback &&resultCallback,
                          RedisExceptionCallback &&exceptionCallback,
                          string_view command,
                          ...) noexcept override;
    ~RedisClientLockFree() override;
    RedisTransactionPtr newTransaction() override
    {
        LOG_ERROR
            << "You can't use the synchronous interface in the fast redis "
               "client, please use the asynchronous version "
               "(newTransactionAsync)";
        assert(0);
        return nullptr;
    }
    void newTransactionAsync(
        const std::function<void(const RedisTransactionPtr &)> &callback)
        override;

    void setTimeout(double timeout) override
    {
        timeout_ = timeout;
    }

  private:
    trantor::EventLoop *loop_;
    std::unordered_set<RedisConnectionPtr> connections_;
    std::vector<RedisConnectionPtr> readyConnections_;
    size_t connectionPos_{0};
    RedisConnectionPtr newConnection();
    const trantor::InetAddress serverAddr_;
    const std::string password_;
    const unsigned int db_;
    const size_t numberOfConnections_;
    std::list<std::shared_ptr<std::function<void(const RedisConnectionPtr &)>>>
        tasks_;
    double timeout_{-1.0};
    std::shared_ptr<RedisTransaction> makeTransaction(
        const RedisConnectionPtr &connPtr);
    void handleNextTask(const RedisConnectionPtr &connPtr);
    void execCommandAsyncWithTimeout(string_view command,
                                     RedisResultCallback &&resultCallback,
                                     RedisExceptionCallback &&exceptionCallback,
                                     va_list ap);
};
}  // namespace nosql
}  // namespace drogon
