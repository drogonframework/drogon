/**
 *
 *  WebsocketControllersRouter.cc
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

#include "WebsocketControllersRouter.h"
#include "WebSocketConnectionImpl.h"
#include "FiltersFunction.h"
#include <drogon/WebSocketController.h>
using namespace drogon;

void WebsocketControllersRouter::registerWebSocketController(
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
    auto &item = wsCtrlMap_[path];
    auto binder = std::make_shared<CtrlBinder>();
    binder->handlerName_ = ctrlName;
    binder->filterNames_ = filters;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<WebSocketControllerBase>(object_);
        assert(controller);
        binder->controller_ = controller;
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

RouteResult WebsocketControllersRouter::route(const HttpRequestImplPtr &req)
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    if (!wsKey.empty())
    {
        std::string pathLower(req->path().length(), 0);
        std::transform(req->path().begin(),
                       req->path().end(),
                       pathLower.begin(),
                       [](unsigned char c) { return tolower(c); });
        auto iter = wsCtrlMap_.find(pathLower);
        if (iter != wsCtrlMap_.end())
        {
            auto &ctrlInfo = iter->second;
            req->setMatchedPathPattern(iter->first);
            auto &binder = ctrlInfo.binders_[req->method()];
            return {true, binder};  // binder maybe null
        }
    }
    return {false, nullptr};
}

std::vector<HttpHandlerInfo> WebsocketControllersRouter::getHandlersInfo() const
{
    std::vector<HttpHandlerInfo> ret;
    for (auto &item : wsCtrlMap_)
    {
        for (size_t i = 0; i < Invalid; ++i)
        {
            if (item.second.binders_[i])
            {
                auto info = std::tuple<std::string, HttpMethod, std::string>(
                    item.first,
                    (HttpMethod)i,
                    std::string("WebsocketController: ") +
                        item.second.binders_[i]->handlerName_);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void WebsocketControllersRouter::init()
{
    // Init filters and corsMethods
    for (auto &iter : wsCtrlMap_)
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
                        corsMethods->append(to_string_view((HttpMethod)i));
                        corsMethods->append(",");
                    }
                }
            }
        }
        corsMethods->pop_back();
    }
}
