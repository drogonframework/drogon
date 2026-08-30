/**
 *
 *  HttpConstraint.h
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

#include <drogon/HttpTypes.h>
#include <string>

namespace drogon
{
namespace internal
{
enum class ConstraintType
{
    None,
    HttpMethod,
    HttpMiddleware
};

class HttpConstraint
{
  public:
    HttpConstraint(HttpMethod method)
        : type_(ConstraintType::HttpMethod), method_(method)
    {
    }

    HttpConstraint(std::string middlewareName)
        : type_(ConstraintType::HttpMiddleware),
          middlewareName_(std::move(middlewareName))
    {
    }

    HttpConstraint(const char *middlewareName)
        : type_(ConstraintType::HttpMiddleware), middlewareName_(middlewareName)
    {
    }

    ConstraintType type() const
    {
        return type_;
    }

    HttpMethod getHttpMethod() const
    {
        return method_;
    }

    const std::string &getMiddlewareName() const
    {
        return middlewareName_;
    }

  private:
    ConstraintType type_{ConstraintType::None};
    HttpMethod method_{HttpMethod::Invalid};
    std::string middlewareName_;
};
}  // namespace internal
}  // namespace drogon
