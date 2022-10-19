/**
 *
 *  WebsocketControllersRouter.h
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
#include <drogon/HttpTypes.h>
#include <drogon/utils/HttpConstraint.h>
#include <drogon/drogon_callbacks.h>
#include <trantor/utils/NonCopyable.h>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <vector>
#include <unordered_map>

namespace drogon
{
class HttpAppFrameworkImpl;
class WebsocketControllersRouter : public trantor::NonCopyable
{
  public:
    WebsocketControllersRouter(
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
        : postRoutingAdvices_(postRoutingAdvices),
          postRoutingObservers_(postRoutingObservers),
          preHandlingAdvices_(preHandlingAdvices),
          preHandlingObservers_(preHandlingObservers),
          postHandlingAdvices_(postHandlingAdvices)
    {
    }
    void registerWebSocketController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               const WebSocketConnectionImplPtr &wsConnPtr);
    void init();

    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

  private:
    struct CtrlBinder
    {
        std::shared_ptr<WebSocketControllerBase> controller_;
        std::string controllerName_;
        std::vector<std::string> filterNames_;
        std::vector<std::shared_ptr<HttpFilterBase>> filters_;
        bool isCORS_{false};
    };

    using CtrlBinderPtr = std::shared_ptr<CtrlBinder>;

    struct WebSocketControllerRouterItem
    {
        CtrlBinderPtr binders_[Invalid];
    };
    std::unordered_map<std::string, WebSocketControllerRouterItem> wsCtrlMap_;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &postRoutingAdvices_;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &postRoutingObservers_;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &preHandlingAdvices_;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &preHandlingObservers_;
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &postHandlingAdvices_;
    void doControllerHandler(
        const WebSocketControllerRouterItem &routerItem,
        std::string &wsKey,
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const WebSocketConnectionImplPtr &wsConnPtr);
    void doPreHandlingAdvices(
        const WebSocketControllerRouterItem &routerItem,
        std::string &wsKey,
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const WebSocketConnectionImplPtr &wsConnPtr);
};
}  // namespace drogon
