/**
 *
 *  @file HttpSimpleControllersRouter.cc
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

#include "HttpSimpleControllersRouter.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpControllersRouter.h"
#include "FiltersFunction.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/HttpSimpleController.h>
#include <drogon/utils/HttpConstraint.h>

using namespace drogon;

void HttpSimpleControllersRouter::registerHttpSimpleController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &filtersAndMethods)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(),
                   pathName.end(),
                   path.begin(),
                   [](unsigned char c) { return tolower(c); });
    std::lock_guard<std::mutex> guard(simpleCtrlMutex_);
    std::vector<HttpMethod> validMethods;
    std::vector<std::string> filters;
    for (auto const &filterOrMethod : filtersAndMethods)
    {
        if (filterOrMethod.type() == internal::ConstraintType::HttpFilter)
        {
            filters.push_back(filterOrMethod.getFilterName());
        }
        else if (filterOrMethod.type() == internal::ConstraintType::HttpMethod)
        {
            validMethods.push_back(filterOrMethod.getHttpMethod());
        }
        else
        {
            LOG_ERROR << "Invalid controller constraint type";
            exit(1);
        }
    }
    auto &item = simpleCtrlMap_[path];
    auto binder = std::make_shared<HttpSimpleControllerBinder>();
    binder->handlerName_ = ctrlName;
    binder->filterNames_ = filters;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<HttpSimpleControllerBase>(object_);
        binder->controller_ = controller;
        // Recreate this with the correct number of threads.
        binder->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });

    if (!validMethods.empty())
    {
        for (auto const &method : validMethods)
        {
            item.binders_[method] = binder;
            if (method == Options)
            {
                binder->isCORS_ = true;
            }
        }
    }
    else
    {
        // All HTTP methods are valid
        for (size_t i = 0; i < Invalid; ++i)
        {
            item.binders_[i] = binder;
        }
        binder->isCORS_ = true;
    }
}

RouteResult HttpSimpleControllersRouter::tryRoute(const HttpRequestImplPtr &req)
{
    std::string pathLower(req->path().length(), 0);
    std::transform(req->path().begin(),
                   req->path().end(),
                   pathLower.begin(),
                   [](unsigned char c) { return tolower(c); });
    auto iter = simpleCtrlMap_.find(pathLower);
    if (iter == simpleCtrlMap_.end())
    {
        return {false, nullptr};
    }

    auto &ctrlInfo = iter->second;
    req->setMatchedPathPattern(iter->first);
    auto &binder = ctrlInfo.binders_[req->method()];
    if (!binder)
    {
        return {true, nullptr};
    }
    return {true, binder};
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpSimpleControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : simpleCtrlMap_)
    {
        for (size_t i = 0; i < Invalid; ++i)
        {
            if (item.second.binders_[i])
            {
                auto info = std::tuple<std::string, HttpMethod, std::string>(
                    item.first,
                    (HttpMethod)i,
                    std::string("HttpSimpleController: ") +
                        item.second.binders_[i]->handlerName_);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

static std::string method_to_string(HttpMethod method)
{
    switch (method)
    {
        case Get:
            return "GET";
        case Post:
            return "POST";
        case Put:
            return "PUT";
        case Delete:
            return "DELETE";
        case Head:
            return "HEAD";
        case Options:
            return "OPTIONS";
        case Patch:
            return "PATCH";
        default:
            return "Invalid";
    }
}

void HttpSimpleControllersRouter::init(
    const std::vector<trantor::EventLoop *> & /*ioLoops*/)
{
    for (auto &iter : simpleCtrlMap_)
    {
        auto corsMethods = std::make_shared<std::string>("OPTIONS,");
        auto &item = iter.second;
        for (size_t i = 0; i < Invalid; ++i)
        {
            auto &binder = item.binders_[i];
            if (binder)
            {
                binder->filters_ =
                    filters_function::createFilters(binder->filterNames_);

                binder->corsMethods_ = corsMethods;
                if (binder->isCORS_)
                {
                    if (i == Get)
                    {
                        corsMethods->append("GET,HEAD,");
                    }
                    else if (i != Options)
                    {
                        corsMethods->append(method_to_string((HttpMethod)i));
                        corsMethods->append(",");
                    }
                }
            }
        }
        corsMethods->pop_back();
    }
}
