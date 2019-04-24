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

#include "FiltersFunction.h"
#include "HttpSimpleControllersRouter.h"
#include "HttpAppFrameworkImpl.h"
#include "AOPAdvice.h"

using namespace drogon;

void HttpSimpleControllersRouter::registerHttpSimpleController(const std::string &pathName,
                                                               const std::string &ctrlName,
                                                               const std::vector<any> &filtersAndMethods)
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
        if (filterOrMethod.type() == typeid(std::string))
        {
            filters.push_back(*any_cast<std::string>(&filterOrMethod));
        }
        else if (filterOrMethod.type() == typeid(const char *))
        {
            filters.push_back(*any_cast<const char *>(&filterOrMethod));
        }
        else if (filterOrMethod.type() == typeid(HttpMethod))
        {
            validMethods.push_back(*any_cast<HttpMethod>(&filterOrMethod));
        }
        else
        {
            std::cerr << "Invalid controller constraint type:" << filterOrMethod.type().name() << std::endl;
            LOG_ERROR << "Invalid controller constraint type";
            exit(1);
        }
    }
    auto &item = _simpCtrlMap[path];
    auto binder = std::make_shared<CtrlBinder>();
    binder->_controllerName = ctrlName;
    binder->_filterNames = filters;
    auto &_object = DrClassMap::getSingleInstance(ctrlName);
    auto controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
    binder->_controller = controller;
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
        //All HTTP methods are valid
        for (size_t i = 0; i < Invalid; i++)
        {
            item._binders[i] = binder;
        }
        binder->_isCORS = true;
    }
}

void HttpSimpleControllersRouter::route(const HttpRequestImplPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback,
                                        bool needSetJsessionid,
                                        std::string &&sessionId)
{
    std::string pathLower(req->path().length(), 0);
    std::transform(req->path().begin(), req->path().end(), pathLower.begin(), tolower);
    auto iter = _simpCtrlMap.find(pathLower);
    if (iter != _simpCtrlMap.end())
    {
        auto &ctrlInfo = iter->second;
        req->setMatchedPathPattern(iter->first);
        auto &binder = ctrlInfo._binders[req->method()];
        if (!binder)
        {
            //Invalid Http Method
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
        //Do post routing advices.
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
                auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
                auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
                FiltersFunction::doFilters(filters, req, callbackPtr, needSetJsessionid, sessionIdPtr, [=, &binder]() mutable {
                    doPreHandlingAdvices(binder, ctrlInfo, req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
                });
            }
            else
            {
                doPreHandlingAdvices(binder, ctrlInfo, req, std::move(callback), needSetJsessionid, std::move(sessionId));
            }
            return;
        }
        else
        {

            auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
            doAdvicesChain(_postRoutingAdvices,
                           0,
                           req,
                           callbackPtr,
                           [callbackPtr, &filters, sessionId = std::move(sessionId), req, needSetJsessionid, &ctrlInfo, this, &binder]() mutable {
                               if (!filters.empty())
                               {
                                   auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
                                   FiltersFunction::doFilters(filters, req, callbackPtr, needSetJsessionid, sessionIdPtr, [=, &binder]() mutable {
                                       doPreHandlingAdvices(binder, ctrlInfo, req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
                                   });
                               }
                               else
                               {
                                   doPreHandlingAdvices(binder, ctrlInfo, req, std::move(*callbackPtr), needSetJsessionid, std::move(sessionId));
                               }
                           });
        }
        return;
    }
    _httpCtrlsRouter.route(req, std::move(callback), needSetJsessionid, std::move(sessionId));
}

void HttpSimpleControllersRouter::doControllerHandler(const CtrlBinderPtr &ctrlBinderPtr,
                                                      const SimpleControllerRouterItem &routerItem,
                                                      const HttpRequestImplPtr &req,
                                                      std::function<void(const HttpResponsePtr &)> &&callback,
                                                      bool needSetJsessionid,
                                                      std::string &&sessionId)
{
    auto &controller = ctrlBinderPtr->_controller;
    if (controller)
    {
        HttpResponsePtr &responsePtr = ctrlBinderPtr->_responsePtrMap[req->getLoop()];
        if (responsePtr && (responsePtr->expiredTime() == 0 || (trantor::Date::now() < responsePtr->creationDate().after(responsePtr->expiredTime()))))
        {
            //use cached response!
            LOG_TRACE << "Use cached response";
            if (!needSetJsessionid)
                invokeCallback(callback, req, responsePtr);
            else
            {
                //make a copy response;
                auto newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                newResp->setExpiredTime(-1); //make it temporary
                newResp->addCookie("JSESSIONID", sessionId);
                invokeCallback(callback, req, newResp);
            }
            return;
        }
        else
        {
            controller->asyncHandleHttpRequest(req, [=, callback = std::move(callback), &ctrlBinderPtr, sessionId = std::move(sessionId)](const HttpResponsePtr &resp) {
                auto newResp = resp;
                if (resp->expiredTime() >= 0)
                {
                    //cache the response;
                    std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                    auto loop = req->getLoop();
                    if (loop->isInLoopThread())
                    {
                        ctrlBinderPtr->_responsePtrMap[loop] = resp;
                    }
                    else
                    {
                        loop->queueInLoop([loop, resp, &ctrlBinderPtr]() {
                            ctrlBinderPtr->_responsePtrMap[loop] = resp;
                        });
                    }
                }
                if (needSetJsessionid)
                {
                    if (resp->expiredTime() >= 0)
                    {
                        //make a copy
                        newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                        newResp->setExpiredTime(-1); //make it temporary
                    }
                    newResp->addCookie("JSESSIONID", sessionId);
                }
                invokeCallback(callback, req, newResp);
            });
        }

        return;
    }
    else
    {
        const std::string &ctrlName = ctrlBinderPtr->_controllerName;
        LOG_ERROR << "can't find controller " << ctrlName;
        auto res = drogon::HttpResponse::newNotFoundResponse();
        if (needSetJsessionid)
            res->addCookie("JSESSIONID", sessionId);
        invokeCallback(callback, req, res);
    }
}

std::vector<std::tuple<std::string, HttpMethod, std::string>> HttpSimpleControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : _simpCtrlMap)
    {
        for (size_t i = 0; i < Invalid; i++)
        {
            if (item.second._binders[i])
            {
                auto info = std::tuple<std::string, HttpMethod, std::string>(item.first,
                                                                             (HttpMethod)i,
                                                                             std::string("HttpSimpleController: ") + item.second._binders[i]->_controllerName);
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void HttpSimpleControllersRouter::init(const std::vector<trantor::EventLoop *> &ioLoops)
{
    for (auto &iter : _simpCtrlMap)
    {
        auto &item = iter.second;
        for (size_t i = 0; i < Invalid; i++)
        {
            auto &binder = item._binders[i];
            if (binder)
            {
                binder->_filters = FiltersFunction::createFilters(binder->_filterNames);
                for (auto ioloop : ioLoops)
                {
                    binder->_responsePtrMap[ioloop] = nullptr;
                }
            }
        }
    }
}

void HttpSimpleControllersRouter::doPreHandlingAdvices(const CtrlBinderPtr &ctrlBinderPtr,
                                                       const SimpleControllerRouterItem &routerItem,
                                                       const HttpRequestImplPtr &req,
                                                       std::function<void(const HttpResponsePtr &)> &&callback,
                                                       bool needSetJsessionid,
                                                       std::string &&sessionId)
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
        doControllerHandler(ctrlBinderPtr, routerItem, req, std::move(callback), needSetJsessionid, std::move(sessionId));
    }
    else
    {
        auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
        auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
        doAdvicesChain(_preHandlingAdvices,
                       0,
                       req,
                       std::make_shared<std::function<void(const HttpResponsePtr &)>>([callbackPtr, needSetJsessionid, sessionIdPtr](const HttpResponsePtr &resp) {
                           if (!needSetJsessionid)
                               (*callbackPtr)(resp);
                           else
                           {
                               resp->addCookie("JSESSIONID", *sessionIdPtr);
                               (*callbackPtr)(resp);
                           }
                       }),
                       [this, ctrlBinderPtr, &routerItem, req, callbackPtr, needSetJsessionid, sessionIdPtr]() {
                           doControllerHandler(ctrlBinderPtr, routerItem, req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
                       });
    }
}