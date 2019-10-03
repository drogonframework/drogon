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
        : _fileRouter(router),
          _postRoutingAdvices(postRoutingAdvices),
          _preHandlingAdvices(preHandlingAdvices),
          _postRoutingObservers(postRoutingObservers),
          _preHandlingObservers(preHandlingObservers),
          _postHandlingAdvices(postHandlingAdvices)
    {
    }
    void init(const std::vector<trantor::EventLoop *> &ioLoops);
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &filters,
                     const std::string &handlerName = "");
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

  private:
    StaticFileRouter &_fileRouter;
    struct CtrlBinder
    {
        internal::HttpBinderBasePtr _binderPtr;
        std::string _handlerName;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
        std::vector<size_t> _parameterPlaces;
        std::map<std::string, size_t> _queryParametersPlaces;
        std::shared_ptr<IOThreadStorage<HttpResponse, false>> _responseCache;
        bool _isCORS = false;
    };
    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;
    struct HttpControllerRouterItem
    {
        std::string _pathParameterPattern;
        std::string _pathPattern;
        std::regex _regex;
        CtrlBinderPtr _binders[Invalid] = {
            nullptr};  // The enum value of Invalid is the http methods number
    };
    std::vector<HttpControllerRouterItem> _ctrlVector;
    std::mutex _ctrlMutex;
    std::regex _ctrlRegex;

    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &_postRoutingAdvices;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>>
        &_preHandlingAdvices;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &_postRoutingObservers;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &_preHandlingObservers;
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &_postHandlingAdvices;

    void doPreHandlingAdvices(
        const CtrlBinderPtr &ctrlBinderPtr,
        const HttpControllerRouterItem &routerItem,
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);

    void doControllerHandler(
        const CtrlBinderPtr &ctrlBinderPtr,
        const HttpControllerRouterItem &routerItem,
        const HttpRequestImplPtr &req,
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
