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
#include <vector>
#include <regex>
#include <string>
#include <mutex>
#include <memory>

namespace drogon
{
class HttpAppFrameworkImpl;
class HttpSimpleControllersRouter : public trantor::NonCopyable
{
  public:
    HttpSimpleControllersRouter(HttpAppFrameworkImpl &app) : _appImpl(app) {}
    void registerHttpSimpleController(const std::string &pathName,
                                      const std::string &ctrlName,
                                      const std::vector<any> &filtersAndMethods);
    bool route(const HttpRequestImplPtr &req,
               const std::function<void(const HttpResponsePtr &)> &callback,
               bool needSetJsessionid,
               const std::string &session_id);

  private:
    HttpAppFrameworkImpl &_appImpl;
    struct SimpleControllerRouterItem
    {
        std::string controllerName;
        std::vector<std::string> filtersName;
        std::vector<int> _validMethodsFlags;
        std::shared_ptr<HttpSimpleControllerBase> controller;
        std::shared_ptr<HttpResponse> responsePtr;
        std::mutex _mutex;
    };
    std::unordered_map<std::string, SimpleControllerRouterItem> _simpCtrlMap;
    std::mutex _simpCtrlMutex;
};
} // namespace drogon
