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

RedisTransactionImpl::RedisTransactionImpl(
    const RedisConnectionPtr &connPtr) noexcept
    : connPtr_(connPtr)
{
}

// void RedisTransactionImpl::cancel()
//{
//    if (isExcutedOrConcelled_)
//        return;
//    isExcutedOrConcelled_ = true;
//    execCommandAsync(
//        "cancel",
//        [](const RedisResult &result) {
//            LOG_TRACE << "Reids transaction cancelled";
//        },
//        [](const RedisException &err) { LOG_ERROR << err.what(); });
//}
void RedisTransactionImpl::execute(RedisResultCallback &&resultCallback,
                                   RedisExceptionCallback &&exceptionCallback)
{
    execCommandAsync(std::move(resultCallback),
                     std::move(exceptionCallback),
                     "execute");
}
void RedisTransactionImpl::execCommandAsync(
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback,
    string_view command,
    ...) noexcept
{
    va_list args;
    va_start(args, command);
    connPtr_->sendvCommand(command,
                           std::move(resultCallback),
                           std::move(exceptionCallback),
                           args);
    va_end(args);
}