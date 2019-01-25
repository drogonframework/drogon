/**
 *
 *  FiltersFunction.cc
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

#include "FiltersFunction.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpFilter.h>

#include <queue>

using namespace drogon;

static void doFilterChains(const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
                                size_t index,
                                const HttpRequestImplPtr &req,
                                const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                                bool needSetJsessionid,
                                const std::shared_ptr<std::string> &sessionIdPtr,
                                std::function<void()> &&missCallback)
{

    if (index < filters.size())
    {
        auto &filter = filters[index];
        filter->doFilter(req,
                         [=](HttpResponsePtr res) {
                             if (needSetJsessionid)
                                 res->addCookie("JSESSIONID", *sessionIdPtr);
                             (*callbackPtr)(res);
                         },
                         [=, &filters, missCallback = std::move(missCallback)]() mutable {
                             doFilterChains(filters, index + 1, req, callbackPtr, needSetJsessionid, sessionIdPtr, std::move(missCallback));
                         });
    }
    else
    {
        missCallback();
    }
}

std::vector<std::shared_ptr<HttpFilterBase>> FiltersFunction::createFilters(const std::vector<std::string> &filterNames)
{
    std::vector<std::shared_ptr<HttpFilterBase>> filters;
    for (auto const &filter : filterNames)
    {
        auto _object = DrClassMap::getSingleInstance(filter);
        auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
        if (_filter)
            filters.push_back(_filter);
        else
        {
            LOG_ERROR << "filter " << filter << " not found";
        }
    }
    return filters;
}

void FiltersFunction::doFilters(const std::vector<std::shared_ptr<HttpFilterBase>> &filters,
                                const HttpRequestImplPtr &req,
                                const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                                bool needSetJsessionid,
                                const std::shared_ptr<std::string> &sessionIdPtr,
                                std::function<void()> &&missCallback)
{

    doFilterChains(filters, 0, req, callbackPtr, needSetJsessionid, sessionIdPtr, std::move(missCallback));
}
