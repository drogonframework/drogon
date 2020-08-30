/**
 *
 *  @file CouchBaseCommand.h
 *  An Tao
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
#include <drogon/nosql/CouchBaseClient.h>
#include <string>
namespace drogon
{
namespace nosql
{
enum class CommandType
{
    kGet = 0,
    kStore
};
class CouchBaseCommand
{
  public:
    virtual ~CouchBaseCommand()
    {
    }
    virtual CommandType type() const = 0;
};
class GetCommand : public CouchBaseCommand
{
  public:
    virtual CommandType type() const override
    {
        return CommandType::kGet;
    }
    GetCommand(const std::string &key,
               CBCallback &&callback,
               ExceptionCallback &&errorCallback)
        : key_(key), callback_(callback), errorCallback_(errorCallback)
    {
    }
    std::string key_;
    CBCallback callback_;
    ExceptionCallback errorCallback_;
};
class StoreCommand : public CouchBaseCommand
{
  public:
    virtual CommandType type() const override
    {
        return CommandType::kStore;
    }
    StoreCommand(const std::string &key,
                 const std::string &value,
                 CBCallback &&callback,
                 ExceptionCallback &&errorCallback)
        : key_(key),
          value_(value),
          callback_(callback),
          errorCallback_(errorCallback)
    {
    }
    std::string key_;
    std::string value_;
    CBCallback callback_;
    ExceptionCallback errorCallback_;
};
}  // namespace nosql
}  // namespace drogon