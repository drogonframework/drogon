/**
 *
 *  @file Result.h
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

#include <memory>
namespace drogon
{
namespace nosql
{
class CouchBaseResultImpl;
using CouchBaseResultImplPtr = std::shared_ptr<CouchBaseResultImpl>;
class CouchBaseResult
{
  public:
    CouchBaseResult(CouchBaseResultImplPtr &&resultPtr)
        : resultPtr_(std::move(resultPtr))
    {
    }
    const std::string &getValue() const;

  private:
    CouchBaseResultImplPtr resultPtr_;
};
}  // namespace nosql
}  // namespace drogon