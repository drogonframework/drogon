/**
 *
 *  @file MiddlewaresFunction.cc
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

#include "MiddlewaresFunction.h"
#include "HttpRequestImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/HttpMiddleware.h>

#include <queue>

namespace drogon
{
namespace middlewares_function
{
static void doFilterChains(
    const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
    size_t index,
    const HttpRequestImplPtr &req,
    std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &&callbackPtr)
{
    if (index < filters.size())
    {
        auto &filter = filters[index];
        filter->doFilter(
            req,
            [/*copy*/ callbackPtr](const HttpResponsePtr &resp) {
                (*callbackPtr)(resp);
            },
            [index, req, callbackPtr, &filters]() mutable {
                auto ioLoop = req->getLoop();
                if (ioLoop && !ioLoop->isInLoopThread())
                {
                    ioLoop->queueInLoop(
                        [&filters,
                         index,
                         req,
                         callbackPtr = std::move(callbackPtr)]() mutable {
                            doFilterChains(filters,
                                           index + 1,
                                           req,
                                           std::move(callbackPtr));
                        });
                }
                else
                {
                    doFilterChains(filters,
                                   index + 1,
                                   req,
                                   std::move(callbackPtr));
                }
            });
    }
    else
    {
        (*callbackPtr)(nullptr);
    }
}

void doFilters(const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
               const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doFilterChains(filters, 0, req, std::move(callbackPtr));
}

/**
 * @brief
 * The middlewares are invoked according to the onion ring model.
 *
 * @param outerCallback The road back to the outer layer of the onion ring.
 * @param innermostHandler The innermost handler at the core of the onion ring.
 *
 * When going through each middleware, the `innermostHandler` is passed down as
 * is, while the `outerCallback` is passed to the user code. User code wraps the
 * outerCallback along with other post processing codes into `userPostCb`, and
 * passes it to the next middleware.
 *
 * When reaching the onion core, the `innermostHandler` is finally called. It's
 * parameter is a function that wraps the original `outerCallback` and all
 * `userPostCb`s.
 */
static void passMiddlewareChains(
    const std::vector<std::shared_ptr<HttpMiddlewareBase>> &middlewares,
    size_t index,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&outerCallback,
    std::function<void(std::function<void(const HttpResponsePtr &)> &&)>
        &&innermostHandler)
{
    if (index < middlewares.size())
    {
        auto &middleware = middlewares[index];
        middleware->invoke(
            req,
            [index,
             req,
             innermostHandler = std::move(innermostHandler),
             &middlewares](std::function<void(const HttpResponsePtr &)>
                               &&userPostCb) mutable {
                // call next middleware
                auto ioLoop = req->getLoop();
                if (ioLoop && !ioLoop->isInLoopThread())
                {
                    ioLoop->queueInLoop(
                        [&middlewares,
                         index,
                         req,
                         innermostHandler = std::move(innermostHandler),
                         userPostCb = std::move(userPostCb)]() mutable {
                            passMiddlewareChains(middlewares,
                                                 index + 1,
                                                 req,
                                                 std::move(userPostCb),
                                                 std::move(innermostHandler)

                            );
                        });
                }
                else
                {
                    passMiddlewareChains(middlewares,
                                         index + 1,
                                         req,
                                         std::move(userPostCb),
                                         std::move(innermostHandler));
                }
            },
            std::move(outerCallback));
    }
    else
    {
        innermostHandler(std::move(outerCallback));
    }
}

std::vector<std::shared_ptr<HttpMiddlewareBase>> createMiddlewares(
    const std::vector<std::string> &middlewareNames)
{
    std::vector<std::shared_ptr<HttpMiddlewareBase>> middlewares;
    for (const auto &name : middlewareNames)
    {
        auto object_ = DrClassMap::getSingleInstance(name);
        if (auto middleware =
                std::dynamic_pointer_cast<HttpMiddlewareBase>(object_))
        {
            middlewares.push_back(middleware);
        }
        else
        {
            LOG_ERROR << "middleware " << name << " not found";
        }
    }
    return middlewares;
}

void passMiddlewares(
    const std::vector<std::shared_ptr<HttpMiddlewareBase>> &middlewares,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&outermostCallback,
    std::function<void(std::function<void(const HttpResponsePtr &)> &&)>
        &&innermostHandler)
{
    passMiddlewareChains(middlewares,
                         0,
                         req,
                         std::move(outermostCallback),
                         std::move(innermostHandler));
}

}  // namespace middlewares_function
}  // namespace drogon
