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
#include "AOPAdvice.h"
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
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
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
    auto binder = std::make_shared<CtrlBinder>();
    binder->controllerName_ = ctrlName;
    binder->filterNames_ = filters;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<HttpSimpleControllerBase>(object_);
        binder->controller_ = controller;
        // Recreate this with the correct number of threads.
        binder->responseCache_ = IOThreadStorage<HttpResponsePtr>();
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

void HttpSimpleControllersRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    std::string pathLower(req->path().length(), 0);
    std::transform(req->path().begin(),
                   req->path().end(),
                   pathLower.begin(),
                   tolower);
    auto iter = simpleCtrlMap_.find(pathLower);
    if (iter != simpleCtrlMap_.end())
    {
        auto &ctrlInfo = iter->second;
        req->setMatchedPathPattern(iter->first);
        auto &binder = ctrlInfo.binders_[req->method()];
        if (!binder)
        {
            // Invalid Http Method
            if (req->method() != Options)
            {
                callback(app().getCustomErrorHandler()(k405MethodNotAllowed));
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
                filters_function::doFilters(
                    filters,
                    req,
                    callbackPtr,
                    [this, &ctrlInfo, req, callbackPtr, &binder]() mutable {
                        doPreHandlingAdvices(binder,
                                             ctrlInfo,
                                             req,
                                             std::move(*callbackPtr));
                    });
            }
            else
            {
                doPreHandlingAdvices(binder,
                                     ctrlInfo,
                                     req,
                                     std::move(callback));
            }
            return;
        }
        else
        {
            auto callbackPtr =
                std::make_shared<std::function<void(const HttpResponsePtr &)>>(
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
                 &binder]() mutable {
                    if (!filters.empty())
                    {
                        filters_function::doFilters(
                            filters,
                            req,
                            callbackPtr,
                            [this,
                             &ctrlInfo,
                             req,
                             callbackPtr,
                             &binder]() mutable {
                                doPreHandlingAdvices(binder,
                                                     ctrlInfo,
                                                     req,
                                                     std::move(*callbackPtr));
                            });
                    }
                    else
                    {
                        doPreHandlingAdvices(binder,
                                             ctrlInfo,
                                             req,
                                             std::move(*callbackPtr));
                    }
                });
        }
        return;
    }
    httpCtrlsRouter_.route(req, std::move(callback));
}

void HttpSimpleControllersRouter::doControllerHandler(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto &controller = ctrlBinderPtr->controller_;
    if (controller)
    {
        auto &responsePtr = *(ctrlBinderPtr->responseCache_);
        if (responsePtr)
        {
            if (responsePtr->expiredTime() == 0 ||
                (trantor::Date::now() <
                 responsePtr->creationDate().after(
                     static_cast<double>(responsePtr->expiredTime()))))
            {
                // use cached response!
                LOG_TRACE << "Use cached response";
                invokeCallback(callback, req, responsePtr);
                return;
            }
            else
            {
                responsePtr.reset();
            }
        }

        controller->asyncHandleHttpRequest(
            req,
            [this, req, callback = std::move(callback), &ctrlBinderPtr](
                const HttpResponsePtr &resp) {
                auto newResp = resp;
                if (resp->expiredTime() >= 0 &&
                    resp->statusCode() != k404NotFound)
                {
                    // cache the response;
                    static_cast<HttpResponseImpl *>(resp.get())
                        ->makeHeaderString();
                    auto loop = req->getLoop();

                    if (loop->isInLoopThread())
                    {
                        ctrlBinderPtr->responseCache_.setThreadData(resp);
                    }
                    else
                    {
                        loop->queueInLoop([resp, &ctrlBinderPtr]() {
                            ctrlBinderPtr->responseCache_.setThreadData(resp);
                        });
                    }
                }
                invokeCallback(callback, req, newResp);
            });

        return;
    }
    else
    {
        const std::string &ctrlName = ctrlBinderPtr->controllerName_;
        LOG_ERROR << "can't find controller " << ctrlName;
        auto res = drogon::HttpResponse::newNotFoundResponse();
        invokeCallback(callback, req, res);
    }
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
                        item.second.binders_[i]->controllerName_);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void HttpSimpleControllersRouter::init(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    for (auto &iter : simpleCtrlMap_)
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

void HttpSimpleControllersRouter::doPreHandlingAdvices(
    const CtrlBinderPtr &ctrlBinderPtr,
    const SimpleControllerRouterItem &routerItem,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
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
    if (!preHandlingObservers_.empty())
    {
        for (auto &observer : preHandlingObservers_)
        {
            observer(req);
        }
    }
    if (preHandlingAdvices_.empty())
    {
        doControllerHandler(ctrlBinderPtr, req, std::move(callback));
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        doAdvicesChain(
            preHandlingAdvices_,
            0,
            req,
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                [req, callbackPtr](const HttpResponsePtr &resp) {
                    HttpAppFrameworkImpl::instance().callCallback(req,
                                                                  resp,
                                                                  *callbackPtr);
                }),
            [this, ctrlBinderPtr, req, callbackPtr]() {
                doControllerHandler(ctrlBinderPtr,
                                    req,
                                    std::move(*callbackPtr));
            });
    }
}

void HttpSimpleControllersRouter::invokeCallback(
    const std::function<void(const HttpResponsePtr &)> &callback,
    const HttpRequestImplPtr &req,
    const HttpResponsePtr &resp)
{
    for (auto &advice : postHandlingAdvices_)
    {
        advice(req, resp);
    }
    HttpAppFrameworkImpl::instance().callCallback(req, resp, callback);
}
