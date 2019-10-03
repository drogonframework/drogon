/**
 *
 *  HttpSimpleControllersRouter.cc
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
    std::lock_guard<std::mutex> guard(_simpCtrlMutex);
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
    auto &item = _simpCtrlMap[path];
    auto binder = std::make_shared<CtrlBinder>();
    binder->_controllerName = ctrlName;
    binder->_filterNames = filters;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &_object = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
        binder->_controller = controller;
        // Recreate this with the correct number of threads.
        binder->_responseCache = IOThreadStorage<HttpResponsePtr>();
    });

    if (validMethods.size() > 0)
    {
        for (auto const &method : validMethods)
        {
            item._binders[method] = binder;
            if (method == Options)
            {
                binder->_isCORS = true;
            }
        }
    }
    else
    {
        // All HTTP methods are valid
        for (size_t i = 0; i < Invalid; i++)
        {
            item._binders[i] = binder;
        }
        binder->_isCORS = true;
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
    auto iter = _simpCtrlMap.find(pathLower);
    if (iter != _simpCtrlMap.end())
    {
        auto &ctrlInfo = iter->second;
        req->setMatchedPathPattern(iter->first);
        auto &binder = ctrlInfo._binders[req->method()];
        if (!binder)
        {
            // Invalid Http Method
            auto res = drogon::HttpResponse::newHttpResponse();
            if (req->method() != Options)
            {
                res->setStatusCode(k405MethodNotAllowed);
            }
            else
            {
                res->setStatusCode(k403Forbidden);
            }
            callback(res);
            return;
        }
        // Do post routing advices.
        if (!_postRoutingObservers.empty())
        {
            for (auto &observer : _postRoutingObservers)
            {
                observer(req);
            }
        }
        auto &filters = ctrlInfo._binders[req->method()]->_filters;
        if (_postRoutingAdvices.empty())
        {
            if (!filters.empty())
            {
                auto callbackPtr = std::make_shared<
                    std::function<void(const HttpResponsePtr &)>>(
                    std::move(callback));
                filters_function::doFilters(
                    filters, req, callbackPtr, [=, &binder]() mutable {
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
                _postRoutingAdvices,
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
                            filters, req, callbackPtr, [=, &binder]() mutable {
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
    _httpCtrlsRouter.route(req, std::move(callback));
}

void HttpSimpleControllersRouter::doControllerHandler(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto &controller = ctrlBinderPtr->_controller;
    if (controller)
    {
        auto &responsePtr = *(ctrlBinderPtr->_responseCache);
        if (responsePtr)
        {
            if (responsePtr->expiredTime() == 0 ||
                (trantor::Date::now() <
                 responsePtr->creationDate().after(responsePtr->expiredTime())))
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
                if (resp->expiredTime() >= 0)
                {
                    // cache the response;
                    static_cast<HttpResponseImpl *>(resp.get())
                        ->makeHeaderString();
                    auto loop = req->getLoop();

                    if (loop->isInLoopThread())
                    {
                        ctrlBinderPtr->_responseCache.setThreadData(resp);
                    }
                    else
                    {
                        loop->queueInLoop([resp, &ctrlBinderPtr]() {
                            ctrlBinderPtr->_responseCache.setThreadData(resp);
                        });
                    }
                }
                invokeCallback(callback, req, newResp);
            });

        return;
    }
    else
    {
        const std::string &ctrlName = ctrlBinderPtr->_controllerName;
        LOG_ERROR << "can't find controller " << ctrlName;
        auto res = drogon::HttpResponse::newNotFoundResponse();
        invokeCallback(callback, req, res);
    }
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpSimpleControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : _simpCtrlMap)
    {
        for (size_t i = 0; i < Invalid; i++)
        {
            if (item.second._binders[i])
            {
                auto info = std::tuple<std::string, HttpMethod, std::string>(
                    item.first,
                    (HttpMethod)i,
                    std::string("HttpSimpleController: ") +
                        item.second._binders[i]->_controllerName);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void HttpSimpleControllersRouter::init(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    for (auto &iter : _simpCtrlMap)
    {
        auto &item = iter.second;
        for (size_t i = 0; i < Invalid; i++)
        {
            auto &binder = item._binders[i];
            if (binder)
            {
                binder->_filters =
                    filters_function::createFilters(binder->_filterNames);
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
        if (routerItem._binders[Get] && routerItem._binders[Get]->_isCORS)
        {
            methods.append("GET,HEAD,");
        }
        if (routerItem._binders[Post] && routerItem._binders[Post]->_isCORS)
        {
            methods.append("POST,");
        }
        if (routerItem._binders[Put] && routerItem._binders[Put]->_isCORS)
        {
            methods.append("PUT,");
        }
        if (routerItem._binders[Delete] && routerItem._binders[Delete]->_isCORS)
        {
            methods.append("DELETE,");
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
        resp->addHeader("Access-Control-Allow-Headers",
                        "x-requested-with,content-type");
        callback(resp);
        return;
    }
    if (!_preHandlingObservers.empty())
    {
        for (auto &observer : _preHandlingObservers)
        {
            observer(req);
        }
    }
    if (_preHandlingAdvices.empty())
    {
        doControllerHandler(ctrlBinderPtr, req, std::move(callback));
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        doAdvicesChain(
            _preHandlingAdvices,
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
    for (auto &advice : _postHandlingAdvices)
    {
        advice(req, resp);
    }
    HttpAppFrameworkImpl::instance().callCallback(req, resp, callback);
}
