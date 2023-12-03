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
    auto binder = std::make_shared<WebsocketControllerBinder>();
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
            if (!binder)
            {
                return {RouteResult::MethodNotAllowed, nullptr};
            }
            return {RouteResult::Success, binder};
        }
    }
    return {RouteResult::NotFound, nullptr};
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

void WebsocketControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[20];
    auto sha1 = trantor::utils::sha1(wsKey.c_str(), wsKey.length());
    memcpy(accKey, &sha1, sizeof(sha1));
    auto base64Key = utils::base64Encode(accKey, sizeof(accKey));
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k101SwitchingProtocols);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", base64Key);
    callback(resp);
}

void WebsocketControllerBinder::handleNewConnection(
    const HttpRequestImplPtr &req,
    const WebSocketConnectionImplPtr &wsConnPtr) const
{
    auto ctrlPtr = controller_;
    assert(ctrlPtr);
    wsConnPtr->setMessageCallback(
        [ctrlPtr](std::string &&message,
                  const WebSocketConnectionImplPtr &connPtr,
                  const WebSocketMessageType &type) {
            ctrlPtr->handleNewMessage(connPtr, std::move(message), type);
        });
    wsConnPtr->setCloseCallback(
        [ctrlPtr](const WebSocketConnectionImplPtr &connPtr) {
            ctrlPtr->handleConnectionClosed(connPtr);
        });
    ctrlPtr->handleNewConnection(req, wsConnPtr);
}
