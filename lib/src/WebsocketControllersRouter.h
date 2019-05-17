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
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "WebSocketConnectionImpl.h"
#include <drogon/HttpFilter.h>
#include <drogon/WebSocketController.h>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <trantor/utils/NonCopyable.h>
#include <vector>

namespace drogon
{
class HttpAppFrameworkImpl;
class WebsocketControllersRouter : public trantor::NonCopyable
{
  public:
    WebsocketControllersRouter()
    {
    }
    void registerWebSocketController(const std::string &pathName,
                                     const std::string &ctrlName,
                                     const std::vector<std::string> &filters);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               const WebSocketConnectionImplPtr &wsConnPtr);
    void init();

    std::vector<std::tuple<std::string, HttpMethod, std::string>> getHandlersInfo() const;

  private:
    struct WebSocketControllerRouterItem
    {
        WebSocketControllerBasePtr _controller;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
    };
    std::unordered_map<std::string, WebSocketControllerRouterItem> _websockCtrlMap;
    std::mutex _websockCtrlMutex;

    void doControllerHandler(const WebSocketControllerBasePtr &ctrlPtr,
                             std::string &wsKey,
                             const HttpRequestImplPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback,
                             const WebSocketConnectionImplPtr &wsConnPtr);
};
}  // namespace drogon