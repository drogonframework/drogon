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
#include <trantor/utils/NonCopyable.h>
#include <drogon/WebSocketController.h>
#include <vector>
#include <regex>
#include <string>
#include <mutex>
#include <memory>

namespace drogon
{
class HttpAppFrameworkImpl;
class WebsocketControllersRouter : public trantor::NonCopyable
{
  public:
    WebsocketControllersRouter(HttpAppFrameworkImpl &app) : _appImpl(app) {}
    void registerWebSocketController(const std::string &pathName,
                                     const std::string &ctrlName,
                                     const std::vector<std::string> &filters);
    void route(const HttpRequestImplPtr &req,
               const std::function<void(const HttpResponsePtr &)> &callback,
               const WebSocketConnectionPtr &wsConnPtr);

  private:
    HttpAppFrameworkImpl &_appImpl;
    struct WebSocketControllerRouterItem
    {
        WebSocketControllerBasePtr controller;
        std::vector<std::string> filtersName;
    };
    std::unordered_map<std::string, WebSocketControllerRouterItem> _websockCtrlMap;
    std::mutex _websockCtrlMutex;
};
} // namespace drogon