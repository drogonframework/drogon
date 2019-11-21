/**
 *
 *  HttpSimpleControllersRouter.h
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
#include <drogon/utils/HttpConstraint.h>
#include <drogon/IOThreadStorage.h>
#include <trantor/utils/NonCopyable.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

namespace drogon
{
class HttpSimpleControllersRouter : public trantor::NonCopyable
{
  public:
    HttpSimpleControllersRouter(
        HttpControllersRouter &httpCtrlRouter,
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
        : httpCtrlsRouter_(httpCtrlRouter),
          postRoutingAdvices_(postRoutingAdvices),
          preHandlingAdvices_(preHandlingAdvices),
          postRoutingObservers_(postRoutingObservers),
          preHandlingObservers_(preHandlingObservers),
          postHandlingAdvices_(postHandlingAdvices)
    {
    }

    void registerHttpSimpleController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
    void init(const std::vector<trantor::EventLoop *> &ioLoops);

    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

  private:
    HttpControllersRouter &httpCtrlsRouter_;
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
    struct CtrlBinder
    {
        std::shared_ptr<HttpSimpleControllerBase> controller_;
        std::string controllerName_;
        std::vector<std::string> filterNames_;
        std::vector<std::shared_ptr<HttpFilterBase>> filters_;
        IOThreadStorage<HttpResponsePtr> responseCache_;
        bool isCORS_{false};
    };

    using CtrlBinderPtr = std::shared_ptr<CtrlBinder>;

    struct SimpleControllerRouterItem
    {
        CtrlBinderPtr binders_[Invalid];
    };
    std::unordered_map<std::string, SimpleControllerRouterItem> simpleCtrlMap_;
    std::mutex simpleCtrlMutex_;

    void doPreHandlingAdvices(
        const CtrlBinderPtr &ctrlBinderPtr,
        const SimpleControllerRouterItem &routerItem,
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void doControllerHandler(
        const CtrlBinderPtr &ctrlBinderPtr,
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void invokeCallback(
        const std::function<void(const HttpResponsePtr &)> &callback,
        const HttpRequestImplPtr &req,
        const HttpResponsePtr &resp);
};
}  // namespace drogon
