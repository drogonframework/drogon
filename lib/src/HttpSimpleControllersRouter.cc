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
    item._controllerName = ctrlName;
    item._filterNames = filters;
    item._validMethodsFlags.clear(); //There may be old data, first clear
    if (validMethods.size() > 0)
    {
        item._validMethodsFlags.resize(Invalid, 0);
        for (auto const &method : validMethods)
        {
            item._validMethodsFlags[method] = 1;
        }
    }
    auto controller = item._controller;
    if (!controller)
    {
        auto &_object = DrClassMap::getSingleInstance(ctrlName);
        controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
        item._controller = controller;
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
        if (!ctrlInfo._validMethodsFlags.empty())
        {
            assert(ctrlInfo._validMethodsFlags.size() > req->method());
            if (ctrlInfo._validMethodsFlags[req->method()] == 0)
            {
                //Invalid Http Method
                auto res = drogon::HttpResponse::newHttpResponse();
                res->setStatusCode(k405MethodNotAllowed);
                callback(res);
                return;
            }
        }
        auto &filters = ctrlInfo._filters;
        if (!filters.empty())
        {
            auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
            auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
            FiltersFunction::doFilters(filters, req, callbackPtr, needSetJsessionid, sessionIdPtr, [=, &ctrlInfo]() mutable {
                doControllerHandler(ctrlInfo, req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
            });
        }
        else
        {
            doControllerHandler(ctrlInfo, req, std::move(callback), needSetJsessionid, std::move(sessionId));
        }
        return;
    }
    _httpCtrlsRouter.route(req, std::move(callback), needSetJsessionid, std::move(sessionId));
}

void HttpSimpleControllersRouter::doControllerHandler(SimpleControllerRouterItem &item,
                                                      const HttpRequestImplPtr &req,
                                                      std::function<void(const HttpResponsePtr &)> &&callback,
                                                      bool needSetJsessionid,
                                                      std::string &&sessionId)
{
    const std::string &ctrlName = item._controllerName;
    auto &controller = item._controller;
    if (controller)
    {
        HttpResponsePtr &responsePtr = item._responsePtrMap[req->getLoop()];
        if (responsePtr && (responsePtr->expiredTime() == 0 || (trantor::Date::now() < responsePtr->creationDate().after(responsePtr->expiredTime()))))
        {
            //use cached response!
            LOG_TRACE << "Use cached response";
            if (!needSetJsessionid)
                callback(responsePtr);
            else
            {
                //make a copy response;
                auto newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                newResp->setExpiredTime(-1); //make it temporary
                newResp->addCookie("JSESSIONID", sessionId);
                callback(newResp);
            }
            return;
        }
        else
        {
            controller->asyncHandleHttpRequest(req, [=, callback = std::move(callback), &item, sessionId = std::move(sessionId)](const HttpResponsePtr &resp) {
                auto newResp = resp;
                if (resp->expiredTime() >= 0)
                {
                    //cache the response;
                    std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                    auto loop = req->getLoop();
                    if (loop->isInLoopThread())
                    {
                        item._responsePtrMap[loop] = resp;
                    }
                    else
                    {
                        loop->queueInLoop([loop, resp, &item]() {
                            item._responsePtrMap[loop] = resp;
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
                callback(newResp);
            });
        }

        return;
    }
    else
    {
        LOG_ERROR << "can't find controller " << ctrlName;
        auto res = drogon::HttpResponse::newNotFoundResponse();
        if (needSetJsessionid)
            res->addCookie("JSESSIONID", sessionId);

        callback(res);
    }
}

void HttpSimpleControllersRouter::init(const std::vector<trantor::EventLoop *> &ioLoops)
{
    for (auto &iter : _simpCtrlMap)
    {
        auto &item = iter.second;
        item._filters = FiltersFunction::createFilters(item._filterNames);
        for (auto ioloop : ioLoops)
        {
            item._responsePtrMap[ioloop] = std::shared_ptr<HttpResponse>();
        }
    }
}