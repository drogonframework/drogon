/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <memory>

namespace drogon
{
typedef std::function<void(HttpResponsePtr)> FilterCallback;
typedef std::function<void()> FilterChainCallback;
class HttpFilterBase : public virtual DrObjectBase
{
  public:
    virtual void doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb) = 0;
    virtual ~HttpFilterBase() {}
};
template <typename T>
class HttpFilter : public DrObject<T>, public HttpFilterBase
{
  public:
    virtual ~HttpFilter() {}
};
} // namespace drogon
