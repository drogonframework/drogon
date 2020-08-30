/**
 *
 *  @file NosqlException.h
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
#include <string>
namespace drogon
{
namespace nosql
{
class NosqlException
{
  public:
    NosqlException(const std::string& message) : message_(message)
    {
    }
    NosqlException(std::string&& message) : message_(std::move(message))
    {
    }
    const std::string& what() const
    {
        return message_;
    }

  private:
    std::string message_;
};
}  // namespace nosql
}  // namespace drogon