/**
 *
 *  HttpControllersRouter.cc
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
#include "HttpControllersRouter.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpAppFrameworkImpl.h"

using namespace drogon;

void HttpControllersRouter::init()
{
    std::string regString;
    for (auto &router : _ctrlVector)
    {
        std::regex reg("\\(\\[\\^/\\]\\*\\)");
        std::string tmp = std::regex_replace(router._pathParameterPattern, reg, "[^/]*");
        router._regex = std::regex(router._pathParameterPattern, std::regex_constants::icase);
        regString.append("(").append(tmp).append(")|");
    }
    if (regString.length() > 0)
        regString.resize(regString.length() - 1); //remove the last '|'
    LOG_TRACE << "regex string:" << regString;
    _ctrlRegex = std::regex(regString, std::regex_constants::icase);
}

void HttpControllersRouter::addHttpPath(const std::string &path,
                                        const internal::HttpBinderBasePtr &binder,
                                        const std::vector<HttpMethod> &validMethods,
                                        const std::vector<std::string> &filters)
{
    //path will be like /api/v1/service/method/{1}/{2}/xxx...
    std::vector<size_t> places;
    std::string tmpPath = path;
    std::string paras = "";
    std::regex regex = std::regex("\\{([0-9]+)\\}");
    std::smatch results;
    auto pos = tmpPath.find("?");
    if (pos != std::string::npos)
    {
        paras = tmpPath.substr(pos + 1);
        tmpPath = tmpPath.substr(0, pos);
    }
    std::string originPath = tmpPath;
    while (std::regex_search(tmpPath, results, regex))
    {
        if (results.size() > 1)
        {
            size_t place = (size_t)std::stoi(results[1].str());
            if (place > binder->paramCount() || place == 0)
            {
                LOG_ERROR << "parameter placeholder(value=" << place << ") out of range (1 to "
                          << binder->paramCount() << ")";
                exit(0);
            }
            places.push_back(place);
        }
        tmpPath = results.suffix();
    }
    std::map<std::string, size_t> parametersPlaces;
    if (!paras.empty())
    {
        std::regex pregex("([^&]*)=\\{([0-9]+)\\}&*");
        while (std::regex_search(paras, results, pregex))
        {
            if (results.size() > 2)
            {
                size_t place = (size_t)std::stoi(results[2].str());
                if (place > binder->paramCount() || place == 0)
                {
                    LOG_ERROR << "parameter placeholder(value=" << place << ") out of range (1 to "
                              << binder->paramCount() << ")";
                    exit(0);
                }
                parametersPlaces[results[1].str()] = place;
            }
            paras = results.suffix();
        }
    }
    auto pathParameterPattern = std::regex_replace(originPath, regex, "([^/]*)");
    auto binderInfo = CtrlBinderPtr(new CtrlBinder);
    binderInfo->_filtersName = filters;
    binderInfo->_binderPtr = binder;
    binderInfo->_parameterPlaces = std::move(places);
    binderInfo->_queryParametersPlaces = std::move(parametersPlaces);
    {
        std::lock_guard<std::mutex> guard(_ctrlMutex);
        for (auto &router : _ctrlVector)
        {
            if (router._pathParameterPattern == pathParameterPattern)
            {
                if (validMethods.size() > 0)
                {
                    for (auto const &method : validMethods)
                    {
                        router._binders[method] = binderInfo;
                    }
                }
                else
                {
                    for (int i = 0; i < Invalid; i++)
                        router._binders[i] = binderInfo;
                }
                return;
            }
        }
    }
    struct HttpControllerRouterItem router;
    router._pathParameterPattern = pathParameterPattern;
    if (validMethods.size() > 0)
    {
        for (auto const &method : validMethods)
        {
            router._binders[method] = binderInfo;
        }
    }
    else
    {
        for (int i = 0; i < Invalid; i++)
            router._binders[i] = binderInfo;
    }
    {
        std::lock_guard<std::mutex> guard(_ctrlMutex);
        _ctrlVector.push_back(std::move(router));
    }
}

void HttpControllersRouter::route(const HttpRequestImplPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&callback,
                                  bool needSetJsessionid,
                                  std::string &&sessionId)
{
    //Find http controller
    if (_ctrlRegex.mark_count() > 0)
    {
        std::smatch result;
        if (std::regex_match(req->path(), result, _ctrlRegex))
        {
            for (size_t i = 1; i < result.size(); i++)
            {
                //TODO: Is there any better way to find the sub-match index without using loop?
                if (!result[i].matched)
                    continue;
                if (result[i].str() == req->path() && i <= _ctrlVector.size())
                {
                    size_t ctlIndex = i - 1;
                    auto &routerItem = _ctrlVector[ctlIndex];
                    //LOG_TRACE << "got http access,regex=" << binder._pathParameterPattern;
                    assert(Invalid > req->method());
                    auto &binder = routerItem._binders[req->method()];
                    if (!binder)
                    {
                        //Invalid Http Method
                        auto res = drogon::HttpResponse::newHttpResponse();
                        res->setStatusCode(k405MethodNotAllowed);
                        callback(res);
                        return;
                    }
                    auto &filters = binder->_filtersName;
                    if (!filters.empty())
                    {
                        auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
                        auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
                        FiltersFunction::doFilters(filters, req, callbackPtr, needSetJsessionid, sessionIdPtr, [=, &binder, &routerItem]() {
                            doControllerHandler(binder, routerItem, req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
                        });
                    }
                    else
                    {
                        doControllerHandler(binder, routerItem, req, std::move(callback), needSetJsessionid, std::move(sessionId));
                    }
                }
            }
        }
        else
        {
            //No controller found
            auto res = drogon::HttpResponse::newNotFoundResponse();
            if (needSetJsessionid)
                res->addCookie("JSESSIONID", sessionId);
            callback(res);
        }
    }
    else
    {
        //No controller found
        auto res = drogon::HttpResponse::newNotFoundResponse();
        if (needSetJsessionid)
            res->addCookie("JSESSIONID", sessionId);
        callback(res);
    }
}

void HttpControllersRouter::doControllerHandler(const CtrlBinderPtr &ctrlBinderPtr,
                                                const HttpControllerRouterItem &routerItem,
                                                const HttpRequestImplPtr &req,
                                                std::function<void(const HttpResponsePtr &)> &&callback,
                                                bool needSetJsessionid,
                                                std::string &&sessionId)
{
    HttpResponsePtr responsePtr;
    {
        std::lock_guard<std::mutex> guard(ctrlBinderPtr->_binderMtx);
        responsePtr = ctrlBinderPtr->_responsePtr;
    }

    if (responsePtr && (responsePtr->expiredTime() == 0 || (trantor::Date::now() < responsePtr->createDate().after(responsePtr->expiredTime()))))
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
    std::vector<std::string> params(ctrlBinderPtr->_parameterPlaces.size());
    std::smatch r;
    if (std::regex_match(req->path(), r, routerItem._regex))
    {
        for (size_t j = 1; j < r.size(); j++)
        {
            size_t place = ctrlBinderPtr->_parameterPlaces[j - 1];
            if (place > params.size())
                params.resize(place);
            params[place - 1] = r[j].str();
            LOG_TRACE << "place=" << place << " para:" << params[place - 1];
        }
    }
    if (ctrlBinderPtr->_queryParametersPlaces.size() > 0)
    {
        auto qureyPara = req->getParameters();
        for (auto const &parameter : qureyPara)
        {
            if (ctrlBinderPtr->_queryParametersPlaces.find(parameter.first) !=
                ctrlBinderPtr->_queryParametersPlaces.end())
            {
                auto place = ctrlBinderPtr->_queryParametersPlaces.find(parameter.first)->second;
                if (place > params.size())
                    params.resize(place);
                params[place - 1] = parameter.second;
            }
        }
    }
    std::list<std::string> paraList;
    for (auto &p : params) ///Use reference
    {
        LOG_TRACE << p;
        paraList.push_back(std::move(p));
    }
    ctrlBinderPtr->_binderPtr->handleHttpRequest(paraList, req, [=, callback = std::move(callback), sessionId = std::move(sessionId)](const HttpResponsePtr &resp) {
        LOG_TRACE << "http resp:needSetJsessionid=" << needSetJsessionid << ";JSESSIONID=" << sessionId;
        auto newResp = resp;
        if (resp->expiredTime() >= 0)
        {
            //cache the response;
            std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
            {
                std::lock_guard<std::mutex> guard(ctrlBinderPtr->_binderMtx);
                ctrlBinderPtr->_responsePtr = resp;
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
    return;
}