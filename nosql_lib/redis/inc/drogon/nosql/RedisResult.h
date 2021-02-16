/**
 *
 *  @file RedisResult.h
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
#include <vector>
#include <string>
#include <memory>

struct redisReply;
namespace drogon
{
namespace nosql
{
enum class RedisResultType
{
    kInteger = 0,
    kString,
    kArray,
    kStatus,
    kNil,
    kError
};
class RedisResult
{
  public:
    RedisResult(redisReply *result) : result_(result)
    {
    }
    ~RedisResult() = default;
    RedisResultType type() const noexcept;
    std::string asString() const noexcept(false);
    std::vector<RedisResult> asArray() const noexcept(false);
    long long asInteger() const noexcept(false);
    std::string getStringForDisplaying() const noexcept;
    std::string getStringForDisplayingWithIndent(
        size_t indent = 0) const noexcept;
    bool isNil() const noexcept;
    operator bool() const
    {
        return !isNil();
    }

  private:
    redisReply *result_;
};
}  // namespace nosql
}  // namespace drogon