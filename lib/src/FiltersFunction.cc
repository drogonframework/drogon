/**
 *
 *  @file FiltersFunction.cc
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

#include "FiltersFunction.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/HttpFilter.h>

#include <queue>

namespace drogon
{
namespace filters_function
{
static void doFilterChains(
    const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
    size_t index,
    const HttpRequestImplPtr &req,
    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &callbackPtr,
    std::function<void()> &&missCallback)
{
    if (index < filters.size())
    {
        auto &filter = filters[index];
        filter->doFilter(
            req,
            [req, callbackPtr](const HttpResponsePtr &res) {
                HttpAppFrameworkImpl::instance().callCallback(req,
                                                              res,
                                                              *callbackPtr);
            },
            [index,
             req,
             callbackPtr,
             &filters,
             missCallback = std::move(missCallback)]() mutable {
                auto ioLoop = req->getLoop();
                if (ioLoop && !ioLoop->isInLoopThread())
                {
                    ioLoop->queueInLoop([index,
                                         req,
                                         callbackPtr,
                                         &filters,
                                         missCallback = std::move(missCallback),
                                         ioLoop]() mutable {
                        doFilterChains(filters,
                                       index + 1,
                                       req,
                                       callbackPtr,
                                       std::move(missCallback));
                    });
                }
                else
                {
                    doFilterChains(filters,
                                   index + 1,
                                   req,
                                   callbackPtr,
                                   std::move(missCallback));
                }
            });
    }
    else
    {
        missCallback();
    }
}

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

std::vector<std::shared_ptr<HttpFilterBase>> createFilters(
    const std::vector<std::string> &filterNames)
{
    std::vector<std::shared_ptr<HttpFilterBase>> filters;
    for (auto const &filter : filterNames)
    {
        auto object_ = DrClassMap::getSingleInstance(filter);
        auto filter_ = std::dynamic_pointer_cast<HttpFilterBase>(object_);
        if (filter_)
            filters.push_back(filter_);
        else
        {
            LOG_ERROR << "filter " << filter << " not found";
        }
    }
    return filters;
}

void doFilters(const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
               const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doFilterChains(filters, 0, req, std::move(callbackPtr));
}

}  // namespace filters_function
}  // namespace drogon
