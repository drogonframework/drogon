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
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "AOPAdvice.h"
#include "WebSocketConnectionImpl.h"
#include "FiltersFunction.h"
#include <drogon/HttpFilter.h>
#include <drogon/WebSocketController.h>
#include <drogon/config.h>
#ifdef OpenSSL_FOUND
#include <openssl/sha.h>
#else
#include "ssl_funcs/Sha1.h"
#endif
using namespace drogon;

void WebsocketControllersRouter::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &filtersAndMethods)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
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
    binder->controllerName_ = ctrlName;
    binder->filterNames_ = filters;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<WebSocketControllerBase>(object_);
        binder->controller_ = controller;
    });

    if (validMethods.size() > 0)
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

void WebsocketControllersRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    if (!wsKey.empty())
    {
        std::string pathLower(req->path().length(), 0);
        std::transform(req->path().begin(),
                       req->path().end(),
                       pathLower.begin(),
                       tolower);
        auto iter = wsCtrlMap_.find(pathLower);
        if (iter != wsCtrlMap_.end())
        {
            auto &ctrlInfo = iter->second;
            req->setMatchedPathPattern(iter->first);
            auto &binder = ctrlInfo.binders_[req->method()];
            if (!binder)
            {
                // Invalid Http Method
                if (req->method() != Options)
                {
                    callback(
                        app().getCustomErrorHandler()(k405MethodNotAllowed));
                }
                else
                {
                    callback(app().getCustomErrorHandler()(k403Forbidden));
                }
                return;
            }
            // Do post routing advices.
            if (!postRoutingObservers_.empty())
            {
                for (auto &observer : postRoutingObservers_)
                {
                    observer(req);
                }
            }
            auto &filters = ctrlInfo.binders_[req->method()]->filters_;
            if (postRoutingAdvices_.empty())
            {
                if (!filters.empty())
                {
                    auto callbackPtr = std::make_shared<
                        std::function<void(const HttpResponsePtr &)>>(
                        std::move(callback));
                    filters_function::doFilters(filters,
                                                req,
                                                callbackPtr,
                                                [wsKey = std::move(wsKey),
                                                 req,
                                                 callbackPtr,
                                                 wsConnPtr,
                                                 &ctrlInfo,
                                                 this]() mutable {
                                                    doControllerHandler(
                                                        ctrlInfo,
                                                        wsKey,
                                                        req,
                                                        std::move(*callbackPtr),
                                                        wsConnPtr);
                                                });
                }
                else
                {
                    doControllerHandler(
                        ctrlInfo, wsKey, req, std::move(callback), wsConnPtr);
                }
                return;
            }
            else
            {
                auto callbackPtr = std::make_shared<
                    std::function<void(const HttpResponsePtr &)>>(
                    std::move(callback));
                doAdvicesChain(
                    postRoutingAdvices_,
                    0,
                    req,
                    callbackPtr,
                    [callbackPtr,
                     &filters,
                     req,
                     &ctrlInfo,
                     this,
                     wsKey = std::move(wsKey),
                     wsConnPtr]() mutable {
                        if (!filters.empty())
                        {
                            filters_function::doFilters(
                                filters,
                                req,
                                callbackPtr,
                                [this,
                                 wsKey = std::move(wsKey),
                                 callbackPtr,
                                 wsConnPtr = std::move(wsConnPtr),
                                 req,
                                 &ctrlInfo]() mutable {
                                    doControllerHandler(ctrlInfo,
                                                        wsKey,
                                                        req,
                                                        std::move(*callbackPtr),
                                                        wsConnPtr);
                                });
                        }
                        else
                        {
                            doControllerHandler(ctrlInfo,
                                                wsKey,
                                                req,
                                                std::move(*callbackPtr),
                                                wsConnPtr);
                        }
                    });
            }
            return;
        }
    }
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
WebsocketControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
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
                        item.second.binders_[i]->controllerName_);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void WebsocketControllersRouter::doControllerHandler(
    const WebSocketControllerRouterItem &routerItem,
    std::string &wsKey,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
        std::string methods = "OPTIONS,";
        if (routerItem.binders_[Get] && routerItem.binders_[Get]->isCORS_)
        {
            methods.append("GET,HEAD,");
        }
        if (routerItem.binders_[Post] && routerItem.binders_[Post]->isCORS_)
        {
            methods.append("POST,");
        }
        if (routerItem.binders_[Put] && routerItem.binders_[Put]->isCORS_)
        {
            methods.append("PUT,");
        }
        if (routerItem.binders_[Delete] && routerItem.binders_[Delete]->isCORS_)
        {
            methods.append("DELETE,");
        }
        if (routerItem.binders_[Patch] && routerItem.binders_[Patch]->isCORS_)
        {
            methods.append("PATCH,");
        }
        methods.resize(methods.length() - 1);
        resp->addHeader("ALLOW", methods);

        auto &origin = req->getHeader("Origin");
        if (origin.empty())
        {
            resp->addHeader("Access-Control-Allow-Origin", "*");
        }
        else
        {
            resp->addHeader("Access-Control-Allow-Origin", origin);
        }
        resp->addHeader("Access-Control-Allow-Methods", methods);
        auto &headers = req->getHeaderBy("access-control-request-headers");
        if (!headers.empty())
        {
            resp->addHeader("Access-Control-Allow-Headers", headers);
        }
        callback(resp);
        return;
    }
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()),
         wsKey.length(),
         accKey);
    auto base64Key = utils::base64Encode(accKey, SHA_DIGEST_LENGTH);
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k101SwitchingProtocols);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", base64Key);
    callback(resp);
    auto ctrlPtr = routerItem.binders_[req->method()]->controller_;
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
    return;
}

void WebsocketControllersRouter::init()
{
    for (auto &iter : wsCtrlMap_)
    {
        auto &item = iter.second;
        for (size_t i = 0; i < Invalid; ++i)
        {
            auto &binder = item.binders_[i];
            if (binder)
            {
                binder->filters_ =
                    filters_function::createFilters(binder->filterNames_);
            }
        }
    }
}