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
    HttpFilter
};

class HttpConstraint
{
  public:
    HttpConstraint(HttpMethod method)
        : _type(ConstraintType::HttpMethod), _method(method)
    {
    }
    HttpConstraint(const std::string &filterName)
        : _type(ConstraintType::HttpFilter), _filterName(filterName)
    {
    }
    HttpConstraint(const char *filterName)
        : _type(ConstraintType::HttpFilter), _filterName(filterName)
    {
    }
    ConstraintType type() const
    {
        return _type;
    }
    HttpMethod getHttpMethod() const
    {
        return _method;
    }
    const std::string &getFilterName() const
    {
        return _filterName;
    }

  private:
    ConstraintType _type = ConstraintType::None;
    HttpMethod _method;
    std::string _filterName;
};
}  // namespace internal
}  // namespace drogon