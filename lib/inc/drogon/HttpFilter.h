/**
 *
 *  HttpFilter.h
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

#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <memory>

/// For more details on the class, see the wiki site (the 'Filter' section)

namespace drogon
{
typedef std::function<void(const HttpResponsePtr &)> FilterCallback;
typedef std::function<void()> FilterChainCallback;
class HttpFilterBase : public virtual DrObjectBase
{
  public:
    /// This virtual function should be overrided in subclasses.
    /**
     * This method is an asynchronous interface, user should return the result
     * via 'FilterCallback' or 'FilterChainCallback'. If @param fcb is called,
     * the response object is send to the client by the callback, and doFilter
     * methods of next filters and the handler registed on the path are not
     * called anymore. If @param fccb is called, the next filter's doFilter
     * method or the handler registered on the path is called.
     */
    virtual void doFilter(const HttpRequestPtr &req,
                          FilterCallback &&fcb,
                          FilterChainCallback &&fccb) = 0;
    virtual ~HttpFilterBase()
    {
    }
};
template <typename T, bool AutoCreation = true>
class HttpFilter : public DrObject<T>, public HttpFilterBase
{
  public:
    static const bool isAutoCreation = AutoCreation;
    virtual ~HttpFilter()
    {
    }
};
}  // namespace drogon
