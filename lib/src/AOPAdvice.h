/**
 *
 *  AOPAdvice.h
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
#include <drogon/drogon_callbacks.h>
#include <deque>
#include <functional>
#include <vector>
#include <memory>

namespace drogon
{
void doAdvicesChain(
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>> &advices,
    size_t index,
    const HttpRequestImplPtr &req,
    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &callbackPtr,
    std::function<void()> &&missCallback);
void doAdvicesChain(
    const std::deque<std::function<void(const HttpRequestPtr &,
                                        AdviceCallback &&,
                                        AdviceChainCallback &&)>> &advices,
    size_t index,
    const HttpRequestImplPtr &req,
    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &callbackPtr,
    std::function<void()> &&missCallback);
}  // namespace drogon
