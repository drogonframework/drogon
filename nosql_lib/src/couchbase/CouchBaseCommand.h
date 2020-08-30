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
struct CouchBaseCommand
{
    CommandType type_;
    std::string key_;
    CBCallback callback_;
    ExceptionCallback errorCallback_;
};
}  // namespace nosql
}  // namespace drogon