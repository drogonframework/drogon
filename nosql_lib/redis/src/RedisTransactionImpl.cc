/**
 *
 *  @file RedisConnection.cc
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

#include "RedisTransactionImpl.h"

using namespace drogon::nosql;

RedisTransactionImpl::RedisTransactionImpl(RedisConnectionPtr connPtr) noexcept
    : connPtr_(std::move(connPtr))
{
}

void RedisTransactionImpl::execute(RedisResultCallback &&resultCallback,
                                   RedisExceptionCallback &&exceptionCallback)
{
    execCommandAsync(
        [thisPtr = shared_from_this(),
         resultCallback =
             std::move(resultCallback)](const RedisResult &result) {
            thisPtr->isExecutedOrCancelled_ = true;
            resultCallback(result);
        },
        std::move(exceptionCallback),
        "EXEC");
}
void RedisTransactionImpl::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    string_view command,
    ...) noexcept
{
    if (isExecutedOrCancelled_)
    {
        exceptionCallback(RedisException(RedisErrorCode::kTransactionCancelled,
                                         "Transaction was cancelled"));
        return;
    }
    va_list args;
    va_start(args, command);
    connPtr_->sendvCommand(
        command,
        std::move(resultCallback),
        [thisPtr = shared_from_this(),
         exceptionCallback =
             std::move(exceptionCallback)](const RedisException &err) {
            LOG_ERROR << err.what();
            thisPtr->isExecutedOrCancelled_ = true;
            exceptionCallback(err);
        },
        args);
    va_end(args);
}

void RedisTransactionImpl::doBegin()
{
    assert(!isExecutedOrCancelled_);
    execCommandAsync([](const RedisResult &result) {},
                     [](const RedisException &err) {},
                     "MULTI");
}

RedisTransactionImpl::~RedisTransactionImpl()
{
    if (!isExecutedOrCancelled_)
    {
        LOG_WARN << "The transaction is not executed before being destroyed";
        connPtr_->sendCommand([](const RedisResult &result) {},
                              [](const RedisException &err) {},
                              "DISCARD");
    }
    LOG_TRACE << "transaction is destroyed";
}