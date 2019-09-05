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
std::vector<std::shared_ptr<HttpFilterBase>> createFilters(
    const std::vector<std::string> &filterNames);
void doFilters(
    const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
    const HttpRequestImplPtr &req,
    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &callbackPtr,
    std::function<void()> &&missCallback);

}  // namespace filters_function
}  // namespace drogon
