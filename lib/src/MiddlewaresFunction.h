/**
 *
 *  MiddlewaresFunction.h
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

#include "impl_forwards.h"
#include <memory>
#include <string>
#include <vector>

namespace drogon
{
namespace middlewares_function
{
// We can not remove old filters api. GlobalFilter still needs it.
// GlobalFilter run filters in advice chains, which does not expose the outer
// response handler, so HttpMiddleware is not suitable for it.
void doFilters(const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
               const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);

std::vector<std::shared_ptr<HttpMiddlewareBase>> createMiddlewares(
    const std::vector<std::string> &middlewareNames);

void passMiddlewares(
    const std::vector<std::shared_ptr<HttpMiddlewareBase>> &middlewares,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&outermostCallback,
    std::function<void(std::function<void(const HttpResponsePtr &)> &&)>
        &&innermostHandler);

}  // namespace middlewares_function
}  // namespace drogon
