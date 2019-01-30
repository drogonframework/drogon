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
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpSimpleController.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/HttpBinder.h>
#include <drogon/HttpFilter.h>
#include <vector>
#include <regex>
#include <string>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <atomic>

namespace drogon
{

class HttpAppFrameworkImpl;
class HttpControllersRouter;
class HttpSimpleControllersRouter : public trantor::NonCopyable
{
  public:
    HttpSimpleControllersRouter(HttpControllersRouter &httpCtrlRouter)
        : _httpCtrlsRouter(httpCtrlRouter) {}
    void registerHttpSimpleController(const std::string &pathName,
                                      const std::string &ctrlName,
                                      const std::vector<any> &filtersAndMethods);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               bool needSetJsessionid,
               std::string &&sessionId);
    void init();

  private:
    HttpControllersRouter &_httpCtrlsRouter;
    struct SimpleControllerRouterItem
    {
        std::string _controllerName;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
        std::vector<int> _validMethodsFlags;
        std::shared_ptr<HttpSimpleControllerBase> _controller;
        std::shared_ptr<HttpResponse> _responsePtr;
        std::atomic_flag _mutex = ATOMIC_FLAG_INIT;
    };
    std::unordered_map<std::string, SimpleControllerRouterItem> _simpCtrlMap;
    std::mutex _simpCtrlMutex;

    void doControllerHandler(SimpleControllerRouterItem &item,
                             const HttpRequestImplPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback,
                             bool needSetJsessionid,
                             std::string &&sessionId);
};
} // namespace drogon
