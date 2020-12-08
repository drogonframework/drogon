/**
 *
 *  @file HttpControllersRouter.cc
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

#include "HttpControllersRouter.h"
#include "AOPAdvice.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "StaticFileRouter.h"
#include "HttpAppFrameworkImpl.h"
#include "FiltersFunction.h"
#include <algorithm>
#include <cctype>
#include <deque>

using namespace drogon;

void HttpControllersRouter::doWhenNoHandlerFound(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->path() == "/" &&
        !HttpAppFrameworkImpl::instance().getHomePage().empty())
    {
        req->setPath("/" + HttpAppFrameworkImpl::instance().getHomePage());
        HttpAppFrameworkImpl::instance().forward(req, std::move(callback));
        return;
    }
    fileRouter_.route(req, std::move(callback));
}

void HttpControllersRouter::init(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    for (auto &router : ctrlVector_)
    {
        router.regex_ = std::regex(router.pathParameterPattern_,
                                   std::regex_constants::icase);
        for (auto &binder : router.binders_)
        {
            if (binder)
            {
                binder->filters_ =
                    filters_function::createFilters(binder->filterNames_);
            }
        }
    }
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : ctrlVector_)
    {
        for (size_t i = 0; i < Invalid; ++i)
        {
            if (item.binders_[i])
            {
                auto description =
                    item.binders_[i]->handlerName_.empty()
                        ? std::string("Handler: ") +
                              item.binders_[i]->binderPtr_->handlerName()
                        : std::string("HttpController: ") +
                              item.binders_[i]->handlerName_;
                auto info = std::tuple<std::string, HttpMethod, std::string>(
                    item.pathPattern_, (HttpMethod)i, std::move(description));
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}

void HttpControllersRouter::addHttpRegex(
    const std::string &regExp,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &filters,
    const std::string &handlerName)
{
    auto binderInfo = std::make_shared<CtrlBinder>();
    binderInfo->filterNames_ = filters;
    binderInfo->handlerName_ = handlerName;
    binderInfo->binderPtr_ = binder;
    drogon::app().getLoop()->queueInLoop([binderInfo]() {
        // Recreate this with the correct number of threads.
        binderInfo->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });
    {
        for (auto &router : ctrlVector_)
        {
            if (router.pathParameterPattern_ == regExp)
            {
                if (validMethods.size() > 0)
                {
                    for (auto const &method : validMethods)
                    {
                        router.binders_[method] = binderInfo;
                        if (method == Options)
                            binderInfo->isCORS_ = true;
                    }
                }
                else
                {
                    binderInfo->isCORS_ = true;
                    for (int i = 0; i < Invalid; ++i)
                        router.binders_[i] = binderInfo;
                }
                return;
            }
        }
    }
    struct HttpControllerRouterItem router;
    router.pathParameterPattern_ = regExp;
    router.pathPattern_ = regExp;
    if (validMethods.size() > 0)
    {
        for (auto const &method : validMethods)
        {
            router.binders_[method] = binderInfo;
            if (method == Options)
                binderInfo->isCORS_ = true;
        }
    }
    else
    {
        binderInfo->isCORS_ = true;
        for (int i = 0; i < Invalid; ++i)
            router.binders_[i] = binderInfo;
    }
    ctrlVector_.push_back(std::move(router));
}
void HttpControllersRouter::addHttpPath(
    const std::string &path,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &filters,
    const std::string &handlerName)
{
    // Path is like /api/v1/service/method/{1}/{2}/xxx...
    std::vector<size_t> places;
    std::string tmpPath = path;
    std::string paras = "";
    std::regex regex = std::regex("\\{([^/]*)\\}");
    std::smatch results;
    auto pos = tmpPath.find('?');
    if (pos != std::string::npos)
    {
        paras = tmpPath.substr(pos + 1);
        tmpPath = tmpPath.substr(0, pos);
    }
    std::string originPath = tmpPath;
    size_t placeIndex = 1;
    while (std::regex_search(tmpPath, results, regex))
    {
        if (results.size() > 1)
        {
            auto result = results[1].str();
            if (!result.empty() &&
                std::all_of(result.begin(), result.end(), [](const char c) {
                    return std::isdigit(c);
                }))
            {
                size_t place = (size_t)std::stoi(result);
                if (place > binder->paramCount() || place == 0)
                {
                    LOG_ERROR << "Parameter placeholder(value=" << place
                              << ") out of range (1 to " << binder->paramCount()
                              << ")";
                    LOG_ERROR << "Path pattern: " << path;
                    exit(1);
                }
                if (!std::all_of(places.begin(),
                                 places.end(),
                                 [place](size_t i) { return i != place; }))
                {
                    LOG_ERROR << "Parameter placeholders are duplicated: index="
                              << place;
                    LOG_ERROR << "Path pattern: " << path;
                    exit(1);
                }
                places.push_back(place);
            }
            else
            {
                std::regex regNumberAndName("([0-9]+):.*");
                std::smatch regexResult;
                if (std::regex_match(result, regexResult, regNumberAndName))
                {
                    assert(regexResult.size() == 2 && regexResult[1].matched);
                    auto num = regexResult[1].str();
                    size_t place = (size_t)std::stoi(num);
                    if (place > binder->paramCount() || place == 0)
                    {
                        LOG_ERROR << "Parameter placeholder(value=" << place
                                  << ") out of range (1 to "
                                  << binder->paramCount() << ")";
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [place](size_t i) { return i != place; }))
                    {
                        LOG_ERROR
                            << "Parameter placeholders are duplicated: index="
                            << place;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    places.push_back(place);
                }
                else
                {
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [placeIndex](size_t i) {
                                         return i != placeIndex;
                                     }))
                    {
                        LOG_ERROR
                            << "Parameter placeholders are duplicated: index="
                            << placeIndex;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    places.push_back(placeIndex);
                }
            }
            ++placeIndex;
        }
        tmpPath = results.suffix();
    }
    std::vector<std::pair<std::string, size_t>> parametersPlaces;
    if (!paras.empty())
    {
        std::regex pregex("([^&]*)=\\{([^&]*)\\}&*");
        while (std::regex_search(paras, results, pregex))
        {
            if (results.size() > 2)
            {
                auto result = results[2].str();
                if (!result.empty() &&
                    std::all_of(result.begin(), result.end(), [](const char c) {
                        return std::isdigit(c);
                    }))
                {
                    size_t place = (size_t)std::stoi(result);
                    if (place > binder->paramCount() || place == 0)
                    {
                        LOG_ERROR << "Parameter placeholder(value=" << place
                                  << ") out of range (1 to "
                                  << binder->paramCount() << ")";
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [place](size_t i) {
                                         return i != place;
                                     }) ||
                        !all_of(parametersPlaces.begin(),
                                parametersPlaces.end(),
                                [place](const std::pair<std::string, size_t>
                                            &item) {
                                    return item.second != place;
                                }))
                    {
                        LOG_ERROR << "Parameter placeholders are "
                                     "duplicated: index="
                                  << place;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    parametersPlaces.emplace_back(results[1].str(), place);
                }
                else
                {
                    std::regex regNumberAndName("([0-9]+):.*");
                    std::smatch regexResult;
                    if (std::regex_match(result, regexResult, regNumberAndName))
                    {
                        assert(regexResult.size() == 2 &&
                               regexResult[1].matched);
                        auto num = regexResult[1].str();
                        size_t place = (size_t)std::stoi(num);
                        if (place > binder->paramCount() || place == 0)
                        {
                            LOG_ERROR << "Parameter placeholder(value=" << place
                                      << ") out of range (1 to "
                                      << binder->paramCount() << ")";
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        if (!std::all_of(places.begin(),
                                         places.end(),
                                         [place](size_t i) {
                                             return i != place;
                                         }) ||
                            !all_of(parametersPlaces.begin(),
                                    parametersPlaces.end(),
                                    [place](const std::pair<std::string, size_t>
                                                &item) {
                                        return item.second != place;
                                    }))
                        {
                            LOG_ERROR << "Parameter placeholders are "
                                         "duplicated: index="
                                      << place;
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        parametersPlaces.emplace_back(results[1].str(), place);
                    }
                    else
                    {
                        if (!std::all_of(places.begin(),
                                         places.end(),
                                         [placeIndex](size_t i) {
                                             return i != placeIndex;
                                         }) ||
                            !all_of(parametersPlaces.begin(),
                                    parametersPlaces.end(),
                                    [placeIndex](
                                        const std::pair<std::string, size_t>
                                            &item) {
                                        return item.second != placeIndex;
                                    }))
                        {
                            LOG_ERROR << "Parameter placeholders are "
                                         "duplicated: index="
                                      << placeIndex;
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        parametersPlaces.emplace_back(results[1].str(),
                                                      placeIndex);
                    }
                }
                ++placeIndex;
            }
            paras = results.suffix();
        }
    }
    auto pathParameterPattern =
        std::regex_replace(originPath, regex, "([^/]*)");
    auto binderInfo = std::make_shared<CtrlBinder>();
    binderInfo->filterNames_ = filters;
    binderInfo->handlerName_ = handlerName;
    binderInfo->binderPtr_ = binder;
    binderInfo->parameterPlaces_ = std::move(places);
    binderInfo->queryParametersPlaces_ = std::move(parametersPlaces);
    drogon::app().getLoop()->queueInLoop([binderInfo]() {
        // Recreate this with the correct number of threads.
        binderInfo->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });
    {
        for (auto &router : ctrlVector_)
        {
            if (router.pathParameterPattern_ == pathParameterPattern)
            {
                if (validMethods.size() > 0)
                {
                    for (auto const &method : validMethods)
                    {
                        router.binders_[method] = binderInfo;
                        if (method == Options)
                            binderInfo->isCORS_ = true;
                    }
                }
                else
                {
                    binderInfo->isCORS_ = true;
                    for (int i = 0; i < Invalid; ++i)
                        router.binders_[i] = binderInfo;
                }
                return;
            }
        }
    }
    struct HttpControllerRouterItem router;
    router.pathParameterPattern_ = pathParameterPattern;
    router.pathPattern_ = path;
    if (validMethods.size() > 0)
    {
        for (auto const &method : validMethods)
        {
            router.binders_[method] = binderInfo;
            if (method == Options)
                binderInfo->isCORS_ = true;
        }
    }
    else
    {
        binderInfo->isCORS_ = true;
        for (int i = 0; i < Invalid; ++i)
            router.binders_[i] = binderInfo;
    }
    ctrlVector_.push_back(std::move(router));
}

void HttpControllersRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Find http controller
    for (auto &routerItem : ctrlVector_)
    {
        std::smatch result;
        auto const &ctrlRegex = routerItem.regex_;
        if (std::regex_match(req->path(), result, ctrlRegex))
        {
            assert(Invalid > req->method());
            req->setMatchedPathPattern(routerItem.pathPattern_);
            auto &binder = routerItem.binders_[req->method()];
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
            if (!postRoutingObservers_.empty())
            {
                for (auto &observer : postRoutingObservers_)
                {
                    observer(req);
                }
            }
            if (postRoutingAdvices_.empty())
            {
                if (!binder->filters_.empty())
                {
                    auto &filters = binder->filters_;
                    auto callbackPtr = std::make_shared<
                        std::function<void(const HttpResponsePtr &)>>(
                        std::move(callback));
                    filters_function::doFilters(
                        filters,
                        req,
                        callbackPtr,
                        [req,
                         callbackPtr,
                         this,
                         &binder,
                         &routerItem,
                         result = std::move(result)]() mutable {
                            doPreHandlingAdvices(binder,
                                                 routerItem,
                                                 req,
                                                 std::move(result),
                                                 std::move(*callbackPtr));
                        });
                }
                else
                {
                    doPreHandlingAdvices(binder,
                                         routerItem,
                                         req,
                                         std::move(result),
                                         std::move(callback));
                }
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
                    [&binder,
                     callbackPtr,
                     req,
                     this,
                     &routerItem,
                     result = std::move(result)]() mutable {
                        if (!binder->filters_.empty())
                        {
                            auto &filters = binder->filters_;
                            filters_function::doFilters(
                                filters,
                                req,
                                callbackPtr,
                                [this,
                                 req,
                                 callbackPtr,
                                 &binder,
                                 &routerItem,
                                 result = std::move(result)]() mutable {
                                    doPreHandlingAdvices(binder,
                                                         routerItem,
                                                         req,
                                                         std::move(result),
                                                         std::move(
                                                             *callbackPtr));
                                });
                        }
                        else
                        {
                            doPreHandlingAdvices(binder,
                                                 routerItem,
                                                 req,
                                                 std::move(result),
                                                 std::move(*callbackPtr));
                        }
                    });
            }
            return;
        }
    }
    // No handler found
    doWhenNoHandlerFound(req, std::move(callback));
}

void HttpControllersRouter::doControllerHandler(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpControllerRouterItem &routerItem,
    const HttpRequestImplPtr &req,
    const std::smatch &matchResult,
    std::function<void(const HttpResponsePtr &)> &&callback)
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

    std::deque<std::string> params(ctrlBinderPtr->parameterPlaces_.size());

    for (size_t j = 1; j < matchResult.size(); ++j)
    {
        if (!matchResult[j].matched)
            continue;
        size_t place = j;
        if (j <= ctrlBinderPtr->parameterPlaces_.size())
        {
            place = ctrlBinderPtr->parameterPlaces_[j - 1];
        }
        if (place > params.size())
            params.resize(place);
        params[place - 1] = matchResult[j].str();
        LOG_TRACE << "place=" << place << " para:" << params[place - 1];
    }

    if (!ctrlBinderPtr->queryParametersPlaces_.empty())
    {
        auto &queryPara = req->getParameters();
        for (auto const &paraPlace : ctrlBinderPtr->queryParametersPlaces_)
        {
            auto place = paraPlace.second;
            if (place > params.size())
                params.resize(place);
            auto iter = queryPara.find(paraPlace.first);
            if (iter != queryPara.end())
            {
                params[place - 1] = iter->second;
            }
            else
            {
                params[place - 1] = std::string{};
            }
        }
    }
    ctrlBinderPtr->binderPtr_->handleHttpRequest(
        params,
        req,
        [this, req, ctrlBinderPtr, callback = std::move(callback)](
            const HttpResponsePtr &resp) {
            if (resp->expiredTime() >= 0 && resp->statusCode() != k404NotFound)
            {
                // cache the response;
                static_cast<HttpResponseImpl *>(resp.get())->makeHeaderString();
                auto loop = req->getLoop();
                if (loop->isInLoopThread())
                {
                    ctrlBinderPtr->responseCache_.setThreadData(resp);
                }
                else
                {
                    req->getLoop()->queueInLoop([resp, &ctrlBinderPtr]() {
                        ctrlBinderPtr->responseCache_.setThreadData(resp);
                    });
                }
            }
            invokeCallback(callback, req, resp);
        });
    return;
}

void HttpControllersRouter::doPreHandlingAdvices(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpControllerRouterItem &routerItem,
    const HttpRequestImplPtr &req,
    std::smatch &&matchResult,
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
        doControllerHandler(
            ctrlBinderPtr, routerItem, req, matchResult, std::move(callback));
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
            [this,
             ctrlBinderPtr,
             &routerItem,
             req,
             callbackPtr,
             result = std::move(matchResult)]() {
                doControllerHandler(ctrlBinderPtr,
                                    routerItem,
                                    req,
                                    result,
                                    std::move(*callbackPtr));
            });
    }
}

void HttpControllersRouter::invokeCallback(
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
