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
        : _httpCtrlsRouter(httpCtrlRouter),
          _postRoutingAdvices(postRoutingAdvices),
          _preHandlingAdvices(preHandlingAdvices),
          _postRoutingObservers(postRoutingObservers),
          _preHandlingObservers(preHandlingObservers),
          _postHandlingAdvices(postHandlingAdvices)
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
    HttpControllersRouter &_httpCtrlsRouter;
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
    struct CtrlBinder
    {
        std::shared_ptr<HttpSimpleControllerBase> _controller;
        std::string _controllerName;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
        std::map<trantor::EventLoop *, std::shared_ptr<HttpResponse>>
            _responsePtrMap;
        bool _isCORS = false;
        bool _hasCachedResponse = false;
    };

    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;

    struct SimpleControllerRouterItem
    {
        CtrlBinderPtr _binders[Invalid] = {nullptr};
    };
    std::unordered_map<std::string, SimpleControllerRouterItem> _simpCtrlMap;
    std::mutex _simpCtrlMutex;

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
