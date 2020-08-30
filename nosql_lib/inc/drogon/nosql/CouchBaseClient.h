/**
 *
 *  @file CouchBaseClient.h
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
#include <drogon/nosql/CouchBaseResult.h>
#include <drogon/nosql/NosqlException.h>
#include <functional>
#include <memory>
#include <string>
namespace drogon
{
namespace nosql
{
using CBCallback = std::function<void(const CouchBaseResult &)>;
using ExceptionCallback = std::function<void(const NosqlException &)>;
class CouchBaseClient
{
  public:
    static std::shared_ptr<CouchBaseClient> newClient(
        const std::string &connectString,
        const std::string &userName = "",
        const std::string &password = "",
        size_t connNum = 1);
    virtual void get(const std::string &key,
                     CBCallback &&callback,
                     ExceptionCallback &&errorCallback) = 0;
    virtual void store(const std::string &key,
                       const std::string &value,
                       CBCallback &&callback,
                       ExceptionCallback &&errorCallback) = 0;
};
using CouchBaseClientPtr = std::shared_ptr<CouchBaseClient>;
}  // namespace nosql
}  // namespace drogon