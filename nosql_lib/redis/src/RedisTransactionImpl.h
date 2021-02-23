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
class RedisTransactionImpl
    : public RedisTransaction,
      public std::enable_shared_from_this<RedisTransactionImpl>
{
  public:
    explicit RedisTransactionImpl(
        const RedisConnectionPtr &connection) noexcept;
    // virtual void cancel() override;
    virtual void execute(RedisResultCallback &&resultCallback,
                         RedisExceptionCallback &&exceptionCallback) override;
    virtual void execCommandAsync(RedisResultCallback &&resultCallback,
                                  RedisExceptionCallback &&exceptionCallback,
                                  string_view command,
                                  ...) noexcept override;
    virtual std::shared_ptr<RedisTransaction> newTransaction() override
    {
        return shared_from_this();
    }
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<RedisTransaction> &)>
            &callback) override
    {
        callback(shared_from_this());
    }
    void doBegin();

  private:
    bool isExcutedOrConcelled_{false};
    RedisConnectionPtr connPtr_;
};
}  // namespace nosql
}  // namespace drogon