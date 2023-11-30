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
#include "CtrlBinderBase.h"
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
class HttpControllersRouter : public trantor::NonCopyable
{
  public:
    HttpControllersRouter() = default;
    void init(const std::vector<trantor::EventLoop *> &ioLoops);
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
    internal::RouteResult tryRoute(const HttpRequestImplPtr &req);
    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

    // TODO: temporarily move to public visibility, fix it before merge
    struct CtrlBinder : public internal::CtrlBinderBase
    {
        internal::HttpBinderBasePtr binderPtr_;
        std::vector<size_t> parameterPlaces_;
        std::vector<std::pair<std::string, size_t>> queryParametersPlaces_;
    };

    using CtrlBinderPtr = std::shared_ptr<CtrlBinder>;

    struct HttpControllerRouterItem
    {
        std::string pathParameterPattern_;
        std::string pathPattern_;
        std::regex regex_;
        CtrlBinderPtr binders_[Invalid]{
            nullptr};  // The enum value of Invalid is the http methods number
    };

  private:
    std::unordered_map<std::string, HttpControllerRouterItem> ctrlMap_;
    std::vector<HttpControllerRouterItem> ctrlVector_;
};
}  // namespace drogon
