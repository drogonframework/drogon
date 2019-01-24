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
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
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
class HttpControllersRouter : public trantor::NonCopyable
{
  public:
    HttpControllersRouter(HttpAppFrameworkImpl &app) : _appImpl(app) {}
    void init();
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &filters);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               bool needSetJsessionid,
               std::string &&sessionId);

  private:
    struct CtrlBinder
    {
        internal::HttpBinderBasePtr _binderPtr;
        std::vector<std::string> _filtersName;
        std::vector<size_t> _parameterPlaces;
        std::map<std::string, size_t> _queryParametersPlaces;
        std::mutex _binderMtx;
        std::shared_ptr<HttpResponse> _responsePtr;
    };
    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;
    struct HttpControllerRouterItem
    {
        std::string _pathParameterPattern;
        std::regex _regex;
        CtrlBinderPtr _binders[Invalid]; //The enum value Invalid is the http methods number
    };
    std::vector<HttpControllerRouterItem> _ctrlVector;
    std::mutex _ctrlMutex;
    std::regex _ctrlRegex;
    HttpAppFrameworkImpl &_appImpl;

    void doControllerHandler(const CtrlBinderPtr &ctrlBinderPtr,
                             const HttpControllerRouterItem &routerItem,
                             const HttpRequestImplPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback,
                             bool needSetJsessionid,
                             std::string &&sessionId);
};
} // namespace drogon