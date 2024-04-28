/**
 *
 *  FiltersFunction.h
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
namespace filters_function
{
// remove later. We still use it in GlobalFilter
std::vector<std::shared_ptr<HttpFilterBase>> createFilters(
    const std::vector<std::string> &filterNames);

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

}  // namespace filters_function
}  // namespace drogon
