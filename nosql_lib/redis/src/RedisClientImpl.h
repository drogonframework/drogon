/**
 *
 *  @file RedisClientImpl.h
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
#include "RedisSubscriberImpl.h"
#include "SubscribeContext.h"
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
class RedisConnection;
using RedisConnectionPtr = std::shared_ptr<RedisConnection>;
class RedisClientImpl final
    : public RedisClient,
      public trantor::NonCopyable,
      public std::enable_shared_from_this<RedisClientImpl>
{
  public:
    RedisClientImpl(const trantor::InetAddress &serverAddress,
                    size_t numberOfConnections,
                    std::string username = "",
                    std::string password = "",
                    unsigned int db = 0);
    void execCommandAsync(RedisResultCallback &&resultCallback,
                          RedisExceptionCallback &&exceptionCallback,
                          string_view command,
                          ...) noexcept override;
    ~RedisClientImpl() override;
    std::shared_ptr<RedisSubscriber> newSubscriber() noexcept override;
    RedisTransactionPtr newTransaction() noexcept(false) override
    {
        std::promise<RedisTransactionPtr> prom;
        auto f = prom.get_future();
        newTransactionAsync([&prom](const RedisTransactionPtr &transPtr) {
            prom.set_value(transPtr);
        });
        auto trans = f.get();
        if (!trans)
        {
            throw RedisException(
                RedisErrorCode::kTimeout,
                "Timeout, no connection available for transaction");
        }
        return trans;
    }
    void newTransactionAsync(
        const std::function<void(const RedisTransactionPtr &)> &callback)
        override;
    void setTimeout(double timeout) override
    {
        timeout_ = timeout;
    }
    void init();
    void closeAll() override;

  private:
    trantor::EventLoopThreadPool loops_;
    std::mutex connectionsMutex_;
    std::unordered_set<RedisConnectionPtr> connections_;
    std::vector<RedisConnectionPtr> readyConnections_;
    size_t connectionPos_{0};
    const trantor::InetAddress serverAddr_;
    const std::string username_;
    const std::string password_;
    const unsigned int db_;
    const size_t numberOfConnections_;
    double timeout_{-1.0};
    std::list<std::shared_ptr<std::function<void(const RedisConnectionPtr &)>>>
        tasks_;

    RedisConnectionPtr newConnection(trantor::EventLoop *loop);
    RedisConnectionPtr newSubscribeConnection(
        trantor::EventLoop *loop,
        const std::shared_ptr<RedisSubscriberImpl> &subscriber);

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
