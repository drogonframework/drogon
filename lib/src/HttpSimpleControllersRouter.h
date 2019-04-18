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
#include "HttpControllersRouter.h"
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

class HttpSimpleControllersRouter : public trantor::NonCopyable
{
  public:
    HttpSimpleControllersRouter(HttpControllersRouter &httpCtrlRouter,
                                const std::deque<std::function<void(const HttpRequestPtr &,
                                                                    const AdviceCallback &,
                                                                    const AdviceChainCallback &)>>
                                    &postRoutingAdvices,
                                const std::deque<std::function<void(const HttpRequestPtr &)>>
                                    &postRoutingObservers,
                                const std::vector<std::function<void(const HttpRequestPtr &,
                                                                     const AdviceCallback &,
                                                                     const AdviceChainCallback &)>>
                                    &preHandlingAdvices,
                                const std::vector<std::function<void(const HttpRequestPtr &)>>
                                    &preHandlingObservers,
                                const std::deque<std::function<void(const HttpRequestPtr &,
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

    void registerHttpSimpleController(const std::string &pathName,
                                      const std::string &ctrlName,
                                      const std::vector<any> &filtersAndMethods);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               bool needSetJsessionid,
               std::string &&sessionId);
    void init(const std::vector<trantor::EventLoop *> &ioLoops);

  private:
    HttpControllersRouter &_httpCtrlsRouter;
    const std::deque<std::function<void(const HttpRequestPtr &,
                                        const AdviceCallback &,
                                        const AdviceChainCallback &)>>
        &_postRoutingAdvices;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         const AdviceCallback &,
                                         const AdviceChainCallback &)>>
        &_preHandlingAdvices;
    const std::deque<std::function<void(const HttpRequestPtr &)>>
        &_postRoutingObservers;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &_preHandlingObservers;

    const std::deque<std::function<void(const HttpRequestPtr &,
                                        const HttpResponsePtr &)>>
        &_postHandlingAdvices;
    struct CtrlBinder
    {
        std::shared_ptr<HttpSimpleControllerBase> _controller;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
        std::map<trantor::EventLoop *, std::shared_ptr<HttpResponse>> _responsePtrMap;
        bool _isCORS = false;
    };
    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;

    struct SimpleControllerRouterItem
    {
        std::string _controllerName;
        CtrlBinderPtr _binders[Invalid] = {nullptr};
    };
    std::unordered_map<std::string, SimpleControllerRouterItem> _simpCtrlMap;
    std::mutex _simpCtrlMutex;

    void doPreHandlingAdvices(const CtrlBinderPtr &ctrlBinderPtr,
                              const SimpleControllerRouterItem &routerItem,
                              const HttpRequestImplPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback,
                              bool needSetJsessionid,
                              std::string &&sessionId);
    void doControllerHandler(const CtrlBinderPtr &ctrlBinderPtr,
                             const SimpleControllerRouterItem &routerItem,
                             const HttpRequestImplPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback,
                             bool needSetJsessionid,
                             std::string &&sessionId);
    void invokeCallback(const std::function<void(const HttpResponsePtr &)> &callback,
                        const HttpRequestImplPtr &req,
                        const HttpResponsePtr &resp)
    {
        for (auto &advice : _postHandlingAdvices)
        {
            advice(req,resp);
        }
        callback(resp);
    }
};
} // namespace drogon
