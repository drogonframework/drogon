/**
 *
 *  HttpControllersRouter.h
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

#pragma once

#include "impl_forwards.h"
#include "HttpControllerBinder.h"
#include <drogon/drogon_callbacks.h>
#include <drogon/HttpBinder.h>
#include <drogon/IOThreadStorage.h>
#include <trantor/utils/NonCopyable.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

namespace drogon
{
class HttpControllerBinder;
class HttpSimpleControllerBinder;

class HttpControllersRouter : public trantor::NonCopyable
{
  public:
    HttpControllersRouter() = default;
    void init(const std::vector<trantor::EventLoop *> &ioLoops);

    void registerHttpSimpleController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods);
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &filters,
                     const std::string &handlerName = "");
    void addHttpRegex(const std::string &regExp,
                      const internal::HttpBinderBasePtr &binder,
                      const std::vector<HttpMethod> &validMethods,
                      const std::vector<std::string> &filters,
                      const std::string &handlerName = "");
    RouteResult route(const HttpRequestImplPtr &req);
    std::vector<HttpHandlerInfo> getHandlersInfo() const;

  private:
    using CtrlBinderPtr = std::shared_ptr<HttpControllerBinder>;

    struct HttpControllerRouterItem
    {
        std::string pathParameterPattern_;
        std::string pathPattern_;
        std::regex regex_;
        CtrlBinderPtr binders_[Invalid]{
            nullptr};  // The enum value of Invalid is the http methods number
    };

    void addRegexCtrlBinder(const CtrlBinderPtr &binderPtr,
                            const std::string &pathPattern,
                            const std::string &pathParameterPattern,
                            const std::vector<HttpMethod> &methods);
    void addCtrlBinderToRouterItem(const CtrlBinderPtr &binderPtr,
                                   HttpControllerRouterItem &router,
                                   const std::vector<HttpMethod> &methods);

    std::unordered_map<std::string, HttpControllerRouterItem> ctrlMap_;
    std::vector<HttpControllerRouterItem> ctrlVector_;

    using SimpleCtrlBinderPtr = std::shared_ptr<HttpSimpleControllerBinder>;

    struct SimpleControllerRouterItem
    {
        SimpleCtrlBinderPtr binders_[Invalid];
    };

    std::unordered_map<std::string, SimpleControllerRouterItem> simpleCtrlMap_;
    std::mutex simpleCtrlMutex_;
};
}  // namespace drogon
