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

void HttpControllersRouter::init(
    const std::vector<trantor::EventLoop *> & /*ioLoops*/)
{
    auto initFiltersAndCorsMethods = [](const HttpControllerRouterItem &item) {
        auto corsMethods = std::make_shared<std::string>("OPTIONS,");
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
    };

    for (auto &router : ctrlVector_)
    {
        router.regex_ = std::regex(router.pathParameterPattern_,
                                   std::regex_constants::icase);
        initFiltersAndCorsMethods(router);
    }

    for (auto &p : ctrlMap_)
    {
        auto &router = p.second;
        router.regex_ = std::regex(router.pathParameterPattern_,
                                   std::regex_constants::icase);
        initFiltersAndCorsMethods(router);
    }
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    auto gatherInfo = [&](const auto &item) {
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
    };
    for (auto &item : ctrlVector_)
    {
        gatherInfo(item);
    }
    for (auto &data : ctrlMap_)
    {
        gatherInfo(data.second);
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
                if (!validMethods.empty())
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
    if (!validMethods.empty())
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
    std::string paras;
    static const std::regex regex("\\{([^/]*)\\}");
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
                auto place = (size_t)std::stoi(result);
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
                static const std::regex regNumberAndName("([0-9]+):.*");
                std::smatch regexResult;
                if (std::regex_match(result, regexResult, regNumberAndName))
                {
                    assert(regexResult.size() == 2 && regexResult[1].matched);
                    auto num = regexResult[1].str();
                    auto place = (size_t)std::stoi(num);
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
        static const std::regex pregex("([^&]*)=\\{([^&]*)\\}&*");
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
                    auto place = (size_t)std::stoi(result);
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
                        auto place = (size_t)std::stoi(num);
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
    bool routingRequiresRegex = (originPath != pathParameterPattern);
    HttpControllerRouterItem *existingRouterItemPtr = nullptr;

    // If exists another controllers on the same route. Updathe them then exit
    if (routingRequiresRegex)
    {
        for (auto &router : ctrlVector_)
        {
            if (router.pathParameterPattern_ == pathParameterPattern)
                existingRouterItemPtr = &router;
        }
    }
    else
    {
        std::string loweredPath;
        loweredPath.resize(originPath.size());
        std::transform(originPath.begin(),
                       originPath.end(),
                       loweredPath.begin(),
                       [](unsigned char c) { return tolower(c); });
        auto it = ctrlMap_.find(loweredPath);
        if (it != ctrlMap_.end())
            existingRouterItemPtr = &it->second;
    }

    if (existingRouterItemPtr != nullptr)
    {
        auto &router = *existingRouterItemPtr;
        if (!validMethods.empty())
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

    struct HttpControllerRouterItem router;
    router.pathParameterPattern_ = pathParameterPattern;
    router.pathPattern_ = path;
    if (!validMethods.empty())
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

    if (routingRequiresRegex)
        ctrlVector_.push_back(std::move(router));
    else
    {
        std::string loweredPath;
        loweredPath.resize(originPath.size());
        std::transform(originPath.begin(),
                       originPath.end(),
                       loweredPath.begin(),
                       [](unsigned char c) { return tolower(c); });
        ctrlMap_[loweredPath] = std::move(router);
    }
}

internal::RouteResult HttpControllersRouter::tryRoute(
    const HttpRequestImplPtr &req)
{
    // Find http controller
    HttpControllerRouterItem *routerItemPtr = nullptr;
    std::smatch result;
    std::string loweredPath = req->path();
    std::transform(loweredPath.begin(),
                   loweredPath.end(),
                   loweredPath.begin(),
                   [](unsigned char c) { return tolower(c); });

    auto it = ctrlMap_.find(loweredPath);
    // Try to find a controller in the hash map. If can't linear search
    // with regex.
    if (it != ctrlMap_.end())
    {
        routerItemPtr = &it->second;
    }
    else
    {
        for (auto &item : ctrlVector_)
        {
            auto const &ctrlRegex = item.regex_;
            if (item.binders_[req->method()] &&
                std::regex_match(req->path(), result, ctrlRegex))
            {
                routerItemPtr = &item;
                break;
            }
        }
    }

    // No handler found
    if (routerItemPtr == nullptr)
    {
        return {false, nullptr};
    }
    HttpControllerRouterItem &routerItem = *routerItemPtr;
    assert(Invalid > req->method());
    req->setMatchedPathPattern(routerItem.pathPattern_);
    auto &binder = routerItem.binders_[req->method()];
    if (!binder)
    {
        return {true, nullptr};
    }
    std::vector<std::string> params;
    for (size_t j = 1; j < result.size(); ++j)
    {
        if (!result[j].matched)
            continue;
        size_t place = j;
        if (j <= binder->parameterPlaces_.size())
        {
            place = binder->parameterPlaces_[j - 1];
        }
        if (place > params.size())
            params.resize(place);
        params[place - 1] = result[j].str();
        LOG_TRACE << "place=" << place << " para:" << params[place - 1];
    }

    if (!binder->queryParametersPlaces_.empty())
    {
        auto &queryPara = req->getParameters();
        for (auto const &paraPlace : binder->queryParametersPlaces_)
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
    req->setRoutingParameters(std::move(params));
    return {true, binder};
}
