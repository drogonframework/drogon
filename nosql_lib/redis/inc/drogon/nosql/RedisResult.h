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
#include <string>
namespace drogon
{
namespace nosql
{
class RedisResult
{
  public:
    virtual ~RedisResult() = default;
    virtual std::string asString() const = 0;
};
}  // namespace nosql
}  // namespace drogon