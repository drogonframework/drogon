/**
 *
 *  @file HttpMiddleware.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
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

#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
/**
 * @brief The abstract base class for middleware
 */
class DROGON_EXPORT HttpMiddlewareBase : public virtual DrObjectBase
{
  public:
    /**
     * This virtual function should be overridden in subclasses.
     *
     * Example:
     * @code
     * void invoke(const HttpRequestPtr &req,
     *             MiddlewareNextCallback &&nextCb,
     *             MiddlewareCallback &&mcb) override
     *  {
     *     if (req->path() == "/some/path") {
     *         // intercept directly
     *         mcb(HttpResponse::newNotFoundResponse(req));
     *         return;
     *     }
     *     // Do something before calling the next middleware
     *     nextCb([mcb = std::move(mcb)](const HttpResponsePtr &resp) {
     *         // Do something after the next middleware returns
     *         mcb(resp);
     *     });
     * }
     * @endcode
     *
     */
    virtual void invoke(const HttpRequestPtr &req,
                        MiddlewareNextCallback &&nextCb,
                        MiddlewareCallback &&mcb) = 0;
    ~HttpMiddlewareBase() override = default;
};

/**
 * @brief The reflection base class template for middlewares
 *
 * @tparam T The type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have non-default constructors.
 */
template <typename T, bool AutoCreation = true>
class HttpMiddleware : public DrObject<T>, public HttpMiddlewareBase
{
  public:
    static constexpr bool isAutoCreation{AutoCreation};
    ~HttpMiddleware() override = default;
};

namespace internal
{
DROGON_EXPORT void handleException(
    const std::exception &,
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &)> &&);
}

#ifdef __cpp_impl_coroutine

struct [[nodiscard]] MiddlewareNextAwaiter
    : public CallbackAwaiter<HttpResponsePtr>
{
  public:
    MiddlewareNextAwaiter(MiddlewareNextCallback &&nextCb)
        : nextCb_(std::move(nextCb))
    {
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept
    {
        nextCb_([this, handle](const HttpResponsePtr &resp) {
            setValue(resp);
            handle.resume();
        });
    }

  private:
    MiddlewareNextCallback nextCb_;
};

template <typename T, bool AutoCreation = true>
class HttpCoroMiddleware : public DrObject<T>, public HttpMiddlewareBase
{
  public:
    static constexpr bool isAutoCreation{AutoCreation};
    ~HttpCoroMiddleware() override = default;

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) final
    {
        drogon::async_run([this,
                           req,
                           nextCb = std::move(nextCb),
                           mcb = std::move(mcb)]() mutable -> drogon::Task<> {
            HttpResponsePtr resp;
            try
            {
                resp = co_await invoke(req, {std::move(nextCb)});
            }
            catch (const std::exception &ex)
            {
                internal::handleException(ex, req, std::move(mcb));
                co_return;
            }
            catch (...)
            {
                LOG_ERROR << "Exception not derived from std::exception";
                co_return;
            }

            mcb(resp);
        });
    }

    virtual Task<HttpResponsePtr> invoke(const HttpRequestPtr &req,
                                         MiddlewareNextAwaiter &&next) = 0;
};

#endif

}  // namespace drogon
