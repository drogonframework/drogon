/**
 *
 *  @file RedisConnection.h
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
#include <memory>

namespace drogon
{
namespace nosql
{
class RedisTransactionImpl final
    : public RedisTransaction,
      public std::enable_shared_from_this<RedisTransactionImpl>
{
  public:
    explicit RedisTransactionImpl(RedisConnectionPtr connection) noexcept;
    // virtual void cancel() override;
    void execute(RedisResultCallback &&resultCallback,
                 RedisExceptionCallback &&exceptionCallback) override;
    void execCommandAsync(RedisResultCallback &&resultCallback,
                          RedisExceptionCallback &&exceptionCallback,
                          std::string_view command,
                          ...) noexcept override;

    std::shared_ptr<RedisSubscriber> newSubscriber() noexcept override
    {
        LOG_ERROR << "You can't create subscriber from redis transaction";
        assert(0);
        return nullptr;
    }

    std::shared_ptr<RedisTransaction> newTransaction() override
    {
        return shared_from_this();
    }

    void newTransactionAsync(
        const std::function<void(const std::shared_ptr<RedisTransaction> &)>
            &callback) override
    {
        callback(shared_from_this());
    }

    void setTimeout(double timeout) override
    {
        timeout_ = timeout;
    }

    void doBegin();
    ~RedisTransactionImpl() override;

  private:
    bool isExecutedOrCancelled_{false};
    RedisConnectionPtr connPtr_;
    double timeout_{-1.0};
};
}  // namespace nosql
}  // namespace drogon
