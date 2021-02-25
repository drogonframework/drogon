/**
 *
 *  @file RedisException.h
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
#include <exception>
#include <functional>

namespace drogon::nosql
{
enum class RedisErrorCode
{
    kNone = 0,
    kUnknown,
    kConnectionBroken,
    kNoConnectionAvailable,
    kRedisError,
    kInternalError,
    kTransactionCancelled
};
class RedisException final : public std::exception
{
  public:
    const char *what() const noexcept override
    {
        return message_.data();
    }
    RedisErrorCode code() const
    {
        return code_;
    }
    RedisException(RedisErrorCode code, const std::string &message)
        : code_(code), message_(message)
    {
    }
    RedisException(RedisErrorCode code, std::string &&message)
        : code_(code), message_(std::move(message))
    {
    }
    RedisException() = delete;

  private:
    std::string message_;
    RedisErrorCode code_{RedisErrorCode::kNone};
};
using RedisExceptionCallback = std::function<void(const RedisException &)>;
}  // namespace drogon::nosql