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
#include <drogon/HttpMiddleware.h>
#include <memory>

#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
/**
 * @brief The abstract base class for filters
 * For more details on the class, see the wiki site (the 'Filter' section)
 */
class DROGON_EXPORT HttpFilterBase : public virtual DrObjectBase,
                                     public HttpMiddlewareBase
{
  public:
    /// This virtual function should be overridden in subclasses.
    /**
     * This method is an asynchronous interface, user should return the result
     * via 'FilterCallback' or 'FilterChainCallback'.
     * @param req is the request object processed by the filter
     * @param fcb if this is called, the response object is send to the client
     * by the callback, and doFilter methods of next filters and the handler
     * registered on the path are not called anymore.
     * @param fccb if this callback is called, the next filter's doFilter method
     * or the handler registered on the path is called.
     */
    virtual void doFilter(const HttpRequestPtr &req,
                          FilterCallback &&fcb,
                          FilterChainCallback &&fccb) = 0;
    ~HttpFilterBase() override = default;

  private:
    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) final
    {
        auto mcbPtr = std::make_shared<MiddlewareCallback>(std::move(mcb));
        doFilter(
            req,
            [mcbPtr](const HttpResponsePtr &resp) {
                (*mcbPtr)(resp);
            },  // fcb, intercept the response
            [nextCb = std::move(nextCb), mcbPtr]() mutable {
                nextCb([mcbPtr = std::move(mcbPtr)](
                           const HttpResponsePtr &resp) { (*mcbPtr)(resp); });
            }  // fccb, call the next middleware
        );
    }
};

/**
 * @brief The reflection base class template for filters
 *
 * @tparam T The type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have non-default constructors.
 */
template <typename T, bool AutoCreation = true>
class HttpFilter : public DrObject<T>, public HttpFilterBase
{
  public:
    static constexpr bool isAutoCreation{AutoCreation};
    ~HttpFilter() override = default;
};

#ifdef __cpp_impl_coroutine
template <typename T, bool AutoCreation = true>
class HttpCoroFilter : public DrObject<T>, public HttpFilterBase
{
  public:
    static constexpr bool isAutoCreation{AutoCreation};
    ~HttpCoroFilter() override = default;

    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) final
    {
        drogon::async_run([this,
                           req,
                           fcb = std::move(fcb),
                           fccb = std::move(fccb)]() mutable -> drogon::Task<> {
            HttpResponsePtr resp;
            try
            {
                resp = co_await doFilter(req);
            }
            catch (const std::exception &ex)
            {
                internal::handleException(ex, req, std::move(fcb));
                co_return;
            }
            catch (...)
            {
                LOG_ERROR << "Exception not derived from std::exception";
                co_return;
            }

            if (resp)
            {
                fcb(resp);
            }
            else
            {
                fccb();
            }
        });
    }

    virtual Task<HttpResponsePtr> doFilter(const HttpRequestPtr &req) = 0;
};
#endif

}  // namespace drogon
