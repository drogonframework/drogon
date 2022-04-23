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
    HttpControllersRouter(
        StaticFileRouter &router,
        const std::vector<std::function<void(const HttpRequestPtr &,
                                             AdviceCallback &&,
                                             AdviceChainCallback &&)>>
            &postRoutingAdvices,
        const std::vector<std::function<void(const HttpRequestPtr &)>>
            &postRoutingObservers,
        const std::vector<std::function<void(const HttpRequestPtr &,
                                             AdviceCallback &&,
                                             AdviceChainCallback &&)>>
            &preHandlingAdvices,
        const std::vector<std::function<void(const HttpRequestPtr &)>>
            &preHandlingObservers,
        const std::vector<std::function<void(const HttpRequestPtr &,
                                             const HttpResponsePtr &)>>
            &postHandlingAdvices)
        : fileRouter_(router),
          postRoutingAdvices_(postRoutingAdvices),
          preHandlingAdvices_(preHandlingAdvices),
          postRoutingObservers_(postRoutingObservers),
          preHandlingObservers_(preHandlingObservers),
          postHandlingAdvices_(postHandlingAdvices)
    {
    }
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
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

  private:
    StaticFileRouter &fileRouter_;
    struct CtrlBinder
    {
        internal::HttpBinderBasePtr binderPtr_;
        std::string handlerName_;
        std::vector<std::string> filterNames_;
        std::vector<std::shared_ptr<HttpFilterBase>> filters_;
        std::vector<size_t> parameterPlaces_;
        std::vector<std::pair<std::string, size_t>> queryParametersPlaces_;
        IOThreadStorage<HttpResponsePtr> responseCache_;
        bool isCORS_{false};
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
    std::unordered_map<std::string, HttpControllerRouterItem> ctrlMap_;
    std::vector<HttpControllerRouterItem> ctrlVector_;

    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &postRoutingAdvices_;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &preHandlingAdvices_;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &postRoutingObservers_;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &preHandlingObservers_;
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &postHandlingAdvices_;

    void doPreHandlingAdvices(
        const CtrlBinderPtr &ctrlBinderPtr,
        const HttpControllerRouterItem &routerItem,
        const HttpRequestImplPtr &req,
        std::smatch &&matchResult,
        std::function<void(const HttpResponsePtr &)> &&callback);

    void doControllerHandler(
        const CtrlBinderPtr &ctrlBinderPtr,
        const HttpControllerRouterItem &routerItem,
        const HttpRequestImplPtr &req,
        const std::smatch &matchResult,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void invokeCallback(
        const std::function<void(const HttpResponsePtr &)> &callback,
        const HttpRequestImplPtr &req,
        const HttpResponsePtr &resp);
    void doWhenNoHandlerFound(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
};
}  // namespace drogon
