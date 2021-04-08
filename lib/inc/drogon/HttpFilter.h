/**
 *
 *  @file HttpFilter.h
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

#include <drogon/DrObject.h>
#include <drogon/drogon_callbacks.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <memory>

namespace drogon
{
/**
 * @brief The abstract base class for filters
 * For more details on the class, see the wiki site (the 'Filter' section)
 */
class DROGON_EXPORT HttpFilterBase : public virtual DrObjectBase
{
  public:
    /// This virtual function should be overrided in subclasses.
    /**
     * This method is an asynchronous interface, user should return the result
     * via 'FilterCallback' or 'FilterChainCallback'.
     * @param req is the request object processed by the filter
     * @param fcb if this is called, the response object is send to the client
     * by the callback, and doFilter methods of next filters and the handler
     * registed on the path are not called anymore.
     * @param fccb if this callback is called, the next filter's doFilter method
     * or the handler registered on the path is called.
     */
    virtual void doFilter(const HttpRequestPtr &req,
                          FilterCallback &&fcb,
                          FilterChainCallback &&fccb) = 0;
    virtual ~HttpFilterBase()
    {
    }
};

/**
 * @brief The reflection base class template for filters
 *
 * @tparam T The type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have nondefault constructors.
 */
template <typename T, bool AutoCreation = true>
class HttpFilter : public DrObject<T>, public HttpFilterBase
{
  public:
    static constexpr bool isAutoCreation{AutoCreation};
    virtual ~HttpFilter()
    {
    }
};
}  // namespace drogon
