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
#include "ControllerBinderBase.h"
#include <drogon/HttpTypes.h>
#include <drogon/HttpAppFramework.h>
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
    WebsocketControllersRouter() = default;

    void registerWebSocketController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods);
    RouteResult route(const HttpRequestImplPtr &req);
    void init();

    std::vector<HttpHandlerInfo> getHandlersInfo() const;

    // TODO: temporarily moved to public visibility
    struct CtrlBinder : public ControllerBinderBase
    {
        std::shared_ptr<WebSocketControllerBase> controller_;

        void handleRequest(
            const HttpRequestImplPtr &req,
            std::function<void(const HttpResponsePtr &)> &&callback) override
        {
            // TODO
        }
    };

    using CtrlBinderPtr = std::shared_ptr<CtrlBinder>;

  private:
    struct WebSocketControllerRouterItem
    {
        CtrlBinderPtr binders_[Invalid];
    };

    std::unordered_map<std::string, WebSocketControllerRouterItem> wsCtrlMap_;
};
}  // namespace drogon
