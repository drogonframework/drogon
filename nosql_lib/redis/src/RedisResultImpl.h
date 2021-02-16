/**
 *
 *  @file RedisResultImpl.h
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
#include <drogon/nosql/RedisResult.h>
#include <hiredis/hiredis.h>
namespace drogon
{
namespace nosql
{
class RedisResultImpl : public RedisResult
{
  public:
    RedisResultImpl(redisReply *result) : result_(result)
    {
    }
    ~RedisResultImpl()
    {
        //  freeReplyObject(result_);
    }
    virtual std::string asString() const override
    {
        return std::string(result_->str);
    }

  private:
    redisReply *result_;
};
}  // namespace nosql
}  // namespace drogon