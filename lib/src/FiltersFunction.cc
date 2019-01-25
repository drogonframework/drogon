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

static void doFilterChain(const std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> &chain,
                          const HttpRequestImplPtr &req,
                          const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                          bool needSetJsessionid,
                          const std::shared_ptr<std::string> &sessionIdPtr,
                          std::function<void()> &&missCallback)
{
    if (chain && chain->size() > 0)
    {
        auto filter = chain->front();
        chain->pop();
        filter->doFilter(req,
                         [=](HttpResponsePtr res) {
                             if (needSetJsessionid)
                                 res->addCookie("JSESSIONID", *sessionIdPtr);
                             (*callbackPtr)(res);
                         },
                         [=, missCallback = std::move(missCallback)]() mutable {
                             doFilterChain(chain, req, callbackPtr, needSetJsessionid, sessionIdPtr, std::move(missCallback));
                         });
    }
    else
    {
        missCallback();
    }
}
void FiltersFunction::doFilters(const std::vector<std::string> &filters,
                                const HttpRequestImplPtr &req,
                                const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                                bool needSetJsessionid,
                                const std::shared_ptr<std::string> &sessionIdPtr,
                                std::function<void()> &&missCallback)
{
    std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> filterPtrs;
    if (!filters.empty())
    {
        filterPtrs = std::make_shared<std::queue<std::shared_ptr<HttpFilterBase>>>();
        for (auto const &filter : filters)
        {
            auto _object = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(filter));
            auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
            if (_filter)
                filterPtrs->push(_filter);
            else
            {
                LOG_ERROR << "filter " << filter << " not found";
            }
        }
    }
    doFilterChain(filterPtrs, req, callbackPtr, needSetJsessionid, sessionIdPtr, std::move(missCallback));
}