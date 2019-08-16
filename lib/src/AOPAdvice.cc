/**
 *
 *  AOPAdvice.cc
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

#include "AOPAdvice.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"

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
    std::function<void()> &&missCallback)
{
    if (index < advices.size())
    {
        auto &advice = advices[index];
        advice(
            req,
            [callbackPtr](const HttpResponsePtr &resp) {
                (*callbackPtr)(resp);
            },
            [index,
             req,
             callbackPtr,
             &advices,
             missCallback = std::move(missCallback)]() mutable {
                doAdvicesChain(advices,
                               index + 1,
                               req,
                               callbackPtr,
                               std::move(missCallback));
            });
    }
    else
    {
        missCallback();
    }
}

void doAdvicesChain(
    const std::deque<std::function<void(const HttpRequestPtr &,
                                        AdviceCallback &&,
                                        AdviceChainCallback &&)>> &advices,
    size_t index,
    const HttpRequestImplPtr &req,
    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &callbackPtr,
    std::function<void()> &&missCallback)
{
    if (index < advices.size())
    {
        auto &advice = advices[index];
        advice(
            req,
            [callbackPtr](const HttpResponsePtr &resp) {
                (*callbackPtr)(resp);
            },
            [index,
             req,
             callbackPtr,
             &advices,
             missCallback = std::move(missCallback)]() mutable {
                doAdvicesChain(advices,
                               index + 1,
                               req,
                               callbackPtr,
                               std::move(missCallback));
            });
    }
    else
    {
        missCallback();
    }
}

}  // namespace drogon