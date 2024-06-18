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
#include "HttpControllerBinder.h"
#include "HttpRequestImpl.h"
#include "HttpAppFrameworkImpl.h"
#include "MiddlewaresFunction.h"
#include <drogon/HttpSimpleController.h>
#include <drogon/WebSocketController.h>
#include <algorithm>

using namespace drogon;

void HttpControllersRouter::init(
    const std::vector<trantor::EventLoop *> & /*ioLoops*/)
{
    auto initMiddlewaresAndCorsMethods = [](const auto &item) {
        auto corsMethods = std::make_shared<std::string>("OPTIONS,");
        for (size_t i = 0; i < Invalid; ++i)
        {
            auto &binder = item.binders_[i];
            if (binder)
            {
                binder->middlewares_ = middlewares_function::createMiddlewares(
                    binder->middlewareNames_);
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
        corsMethods->pop_back();  // remove last comma
    };

    for (auto &iter : simpleCtrlMap_)
    {
        initMiddlewaresAndCorsMethods(iter.second);
    }

    for (auto &iter : wsCtrlMap_)
    {
        initMiddlewaresAndCorsMethods(iter.second);
    }

    for (auto &router : ctrlVector_)
    {
        router.regex_ = std::regex(router.pathParameterPattern_,
                                   std::regex_constants::icase);
        initMiddlewaresAndCorsMethods(router);
    }

    for (auto &p : ctrlMap_)
    {
        auto &router = p.second;
        router.regex_ = std::regex(router.pathParameterPattern_,
                                   std::regex_constants::icase);
        initMiddlewaresAndCorsMethods(router);
    }
}

void HttpControllersRouter::reset()
{
    simpleCtrlMap_.clear();
    ctrlMap_.clear();
    ctrlVector_.clear();
    wsCtrlMap_.clear();
}

std::vector<HttpHandlerInfo> HttpControllersRouter::getHandlersInfo() const
{
    std::vector<HttpHandlerInfo> ret;
    auto gatherInfo = [&ret](const std::string &path, const auto &item) {
        for (size_t i = 0; i < Invalid; ++i)
        {
            if (item.binders_[i])
            {
                std::string description;
                if constexpr (std::is_same_v<std::decay_t<decltype(item)>,
                                             SimpleControllerRouterItem>)

                {
                    description = std::string("HttpSimpleController: ") +
                                  item.binders_[i]->handlerName_;
                }
                else if constexpr (std::is_same_v<
                                       std::decay_t<decltype(item)>,
                                       WebSocketControllerRouterItem> ||
                                   std::is_same_v<
                                       std::decay_t<decltype(item)>,
                                       RegExWebSocketControllerRouterItem>)
                {
                    description = std::string("WebsocketController: ") +
                                  item.binders_[i]->handlerName_;
                }
                else
                {
                    description =
                        item.binders_[i]->handlerName_.empty()
                            ? std::string("Handler: ") +
                                  item.binders_[i]->binderPtr_->handlerName()
                            : std::string("HttpController: ") +
                                  item.binders_[i]->handlerName_;
                }
                ret.emplace_back(path, (HttpMethod)i, std::move(description));
            }
        }
    };

    for (auto &[path, item] : simpleCtrlMap_)
    {
        gatherInfo(path, item);
    }
    for (auto &item : ctrlVector_)
    {
        gatherInfo(item.pathPattern_, item);
    }
    for (auto &[key, item] : ctrlMap_)
    {
        gatherInfo(item.pathPattern_, item);
    }
    for (auto &[path, item] : wsCtrlMap_)
    {
        gatherInfo(path, item);
    }
    for (auto &item : wsCtrlVector_)
    {
        gatherInfo(item.pathPattern_, item);
    }
    return ret;
}

template <typename Binder, typename RouterItem>
static void addCtrlBinderToRouterItem(const std::shared_ptr<Binder> &binderPtr,
                                      RouterItem &router,
                                      const std::vector<HttpMethod> &methods)
{
    if (!methods.empty())
    {
        for (const auto &method : methods)
        {
            router.binders_[method] = binderPtr;
            if (method == Options)
            {
                binderPtr->isCORS_ = true;
            }
        }
    }
    else
    {
        // All HTTP methods are valid
        binderPtr->isCORS_ = true;
        for (int i = 0; i < Invalid; ++i)
        {
            router.binders_[i] = binderPtr;
        }
    }
}

struct SimpleControllerProcessResult
{
    std::string lowerPath;
    std::vector<HttpMethod> validMethods;
    std::vector<std::string> middlewares;
};

static SimpleControllerProcessResult processSimpleControllerParams(
    const std::string &pathName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    std::string path(pathName);
    std::transform(pathName.begin(),
                   pathName.end(),
                   path.begin(),
                   [](unsigned char c) { return tolower(c); });
    std::vector<HttpMethod> validMethods;
    std::vector<std::string> middlewareNames;
    for (const auto &constraint : constraints)
    {
        if (constraint.type() == internal::ConstraintType::HttpMiddleware)
        {
            middlewareNames.push_back(constraint.getMiddlewareName());
        }
        else if (constraint.type() == internal::ConstraintType::HttpMethod)
        {
            validMethods.push_back(constraint.getHttpMethod());
        }
        else
        {
            LOG_ERROR << "Invalid controller constraint type";
            // Used to call exit() here, but that's not very nice.
        }
    }
    return {
        std::move(path),
        std::move(validMethods),
        std::move(middlewareNames),
    };
}

void HttpControllersRouter::registerHttpSimpleController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    // Note: some compiler version failed to handle structural bindings with
    // lambda capture
    auto result = processSimpleControllerParams(pathName, constraints);
    std::string path = std::move(result.lowerPath);

    auto &item = simpleCtrlMap_[path];
    auto binder = std::make_shared<HttpSimpleControllerBinder>();
    binder->handlerName_ = ctrlName;
    binder->middlewareNames_ = result.middlewares;
    drogon::app().getLoop()->queueInLoop([this, binder, ctrlName, path]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<HttpSimpleControllerBase>(object_);
        if (!controller)
        {
            LOG_ERROR << "Controller class not found: " << ctrlName;
            simpleCtrlMap_.erase(path);
            return;
        }
        binder->controller_ = controller;
        // Recreate this with the correct number of threads.
        binder->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });

    addCtrlBinderToRouterItem(binder, item, result.validMethods);
}

void HttpControllersRouter::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    auto result = processSimpleControllerParams(pathName, constraints);
    std::string path = std::move(result.lowerPath);

    auto &item = wsCtrlMap_[path];
    auto binder = std::make_shared<WebsocketControllerBinder>();
    binder->handlerName_ = ctrlName;
    binder->middlewareNames_ = result.middlewares;
    drogon::app().getLoop()->queueInLoop([this, binder, ctrlName, path]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<WebSocketControllerBase>(object_);
        if (!controller)
        {
            LOG_ERROR << "Websocket controller class not found: " << ctrlName;
            wsCtrlMap_.erase(path);
            return;
        }

        binder->controller_ = controller;
    });

    addCtrlBinderToRouterItem(binder, item, result.validMethods);
}

void HttpControllersRouter::registerWebSocketControllerRegex(
    const std::string &regExp,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!regExp.empty());
    assert(!ctrlName.empty());
    auto result = processSimpleControllerParams(regExp, constraints);
    auto binder = std::make_shared<WebsocketControllerBinder>();
    binder->handlerName_ = ctrlName;
    binder->middlewareNames_ = result.middlewares;
    drogon::app().getLoop()->queueInLoop([binder, ctrlName]() {
        auto &object_ = DrClassMap::getSingleInstance(ctrlName);
        auto controller =
            std::dynamic_pointer_cast<WebSocketControllerBase>(object_);
        binder->controller_ = controller;
    });
    struct RegExWebSocketControllerRouterItem router;
    router.pathPattern_ = regExp;
    router.regex_ = regExp;
    addCtrlBinderToRouterItem(binder, router, result.validMethods);
    wsCtrlVector_.push_back(std::move(router));
}

void HttpControllersRouter::addHttpRegex(
    const std::string &regExp,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &middlewareNames,
    const std::string &handlerName)
{
    auto binderInfo = std::make_shared<HttpControllerBinder>();
    binderInfo->middlewareNames_ = middlewareNames;
    binderInfo->handlerName_ = handlerName;
    binderInfo->binderPtr_ = binder;
    drogon::app().getLoop()->queueInLoop([binderInfo]() {
        // Recreate this with the correct number of threads.
        binderInfo->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });

    addRegexCtrlBinder(binderInfo, regExp, regExp, validMethods);
}

void HttpControllersRouter::addHttpPath(
    const std::string &path,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &middlewareNames,
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
    // Process path parameter placeholders
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
    // Process query parameter placeholders
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

    // Create new ControllerBinder
    auto binderInfo = std::make_shared<HttpControllerBinder>();
    binderInfo->middlewareNames_ = middlewareNames;
    binderInfo->handlerName_ = handlerName;
    binderInfo->binderPtr_ = binder;
    binderInfo->parameterPlaces_ = std::move(places);
    binderInfo->queryParametersPlaces_ = std::move(parametersPlaces);
    drogon::app().getLoop()->queueInLoop([binderInfo]() {
        // Recreate this with the correct number of threads.
        binderInfo->responseCache_ = IOThreadStorage<HttpResponsePtr>();
    });

    // Create or update RouterItem
    auto pathParameterPattern =
        std::regex_replace(originPath, regex, "([^/]*)");
    if (originPath != pathParameterPattern)  // require regex
    {
        addRegexCtrlBinder(binderInfo,
                           path,
                           pathParameterPattern,
                           validMethods);
        return;
    }

    std::string loweredPath;
    std::transform(originPath.begin(),
                   originPath.end(),
                   std::back_inserter(loweredPath),
                   [](unsigned char c) { return tolower(c); });

    HttpControllerRouterItem *routerItemPtr;
    // If exists another controllers on the same route, update them
    auto it = ctrlMap_.find(loweredPath);
    if (it != ctrlMap_.end())
    {
        routerItemPtr = &it->second;
    }
    // Create new router item if not exists
    else
    {
        struct HttpControllerRouterItem router;
        router.pathParameterPattern_ = pathParameterPattern;
        router.pathPattern_ = path;
        routerItemPtr =
            &ctrlMap_.emplace(loweredPath, std::move(router)).first->second;
    }

    addCtrlBinderToRouterItem(binderInfo, *routerItemPtr, validMethods);
}

RouteResult HttpControllersRouter::route(const HttpRequestImplPtr &req)
{
    // Find simple controller
    std::string loweredPath(req->path().length(), 0);
    std::transform(req->path().begin(),
                   req->path().end(),
                   loweredPath.begin(),
                   [](unsigned char c) { return tolower(c); });
    {
        auto it = simpleCtrlMap_.find(loweredPath);
        if (it != simpleCtrlMap_.end())
        {
            auto &ctrlInfo = it->second;
            req->setMatchedPathPattern(it->first);
            auto &binder = ctrlInfo.binders_[req->method()];
            if (!binder)
            {
                return {RouteResult::MethodNotAllowed, nullptr};
            }
            return {RouteResult::Success, binder};
        }
    }

    // Find http controller
    HttpControllerRouterItem *routerItemPtr = nullptr;
    std::smatch result;
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
            const auto &ctrlRegex = item.regex_;
            if (item.binders_[req->method()] &&
                std::regex_match(req->path(), result, ctrlRegex))
            {
                routerItemPtr = &item;
                break;
            }
        }
    }

    // No handler found
    if (!routerItemPtr)
    {
        return {RouteResult::NotFound, nullptr};
    }
    HttpControllerRouterItem &routerItem = *routerItemPtr;
    assert(Invalid > req->method());
    req->setMatchedPathPattern(routerItem.pathPattern_);
    auto &binder = routerItem.binders_[req->method()];
    if (!binder)
    {
        return {RouteResult::MethodNotAllowed, nullptr};
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
        for (const auto &paraPlace : binder->queryParametersPlaces_)
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
    return {RouteResult::Success, binder};
}

RouteResult HttpControllersRouter::routeWs(const HttpRequestImplPtr &req)
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
        else
        {
            for (auto &ctrlInfo : wsCtrlVector_)
            {
                auto const &wsCtrlRegex = ctrlInfo.regex_;
                std::smatch result;
                if (std::regex_match(req->path(), result, wsCtrlRegex))
                {
                    req->setMatchedPathPattern(ctrlInfo.pathPattern_);
                    auto &binder = ctrlInfo.binders_[req->method()];
                    if (!binder)
                    {
                        return {RouteResult::MethodNotAllowed, nullptr};
                    }
                    return {RouteResult::Success, binder};
                }
            }
        }
    }
    return {RouteResult::NotFound, nullptr};
}

void HttpControllersRouter::addRegexCtrlBinder(
    const std::shared_ptr<HttpControllerBinder> &binderPtr,
    const std::string &pathPattern,
    const std::string &pathParameterPattern,
    const std::vector<HttpMethod> &methods)
{
    HttpControllerRouterItem *routerItemPtr;
    auto existRouter = std::find_if(ctrlVector_.begin(),
                                    ctrlVector_.end(),
                                    [&pathParameterPattern](const auto &item) {
                                        return item.pathParameterPattern_ ==
                                               pathParameterPattern;
                                    });
    if (existRouter == ctrlVector_.end())
    {
        struct HttpControllerRouterItem router;
        router.pathParameterPattern_ = pathParameterPattern;
        router.pathPattern_ = pathPattern;
        ctrlVector_.push_back(std::move(router));
        routerItemPtr = &ctrlVector_.back();
    }
    else
    {
        routerItemPtr = &(*existRouter);
    }

    addCtrlBinderToRouterItem(binderPtr, *routerItemPtr, methods);
}
