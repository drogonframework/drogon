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
               const std::function<void(const HttpResponsePtr &)> &callback,
               bool needSetJsessionid,
               const std::string &session_id);

  private:
    struct CtrlBinder
    {
        internal::HttpBinderBasePtr binderPtr;
        std::vector<std::string> filtersName;
        std::vector<size_t> parameterPlaces;
        std::map<std::string, size_t> queryParametersPlaces;
        std::unique_ptr<std::mutex> binderMtx = std::unique_ptr<std::mutex>(new std::mutex);
        std::shared_ptr<HttpResponse> responsePtr;
    };
    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;
    struct HttpControllerRouterItem
    {
        std::string pathParameterPattern;
        std::regex _regex;
        CtrlBinderPtr _binders[Invalid]; //The enum value Invalid is the http methods number
    };
    std::vector<HttpControllerRouterItem> _ctrlVector;
    std::mutex _ctrlMutex;
    std::regex _ctrlRegex;
    HttpAppFrameworkImpl &_appImpl;
};
} // namespace drogon