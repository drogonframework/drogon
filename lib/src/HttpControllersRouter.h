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
#include "AOPAdvice.h"
#include <trantor/utils/NonCopyable.h>
#include <drogon/HttpBinder.h>
#include <drogon/HttpFilter.h>
#include <vector>
#include <regex>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>

namespace drogon
{
class HttpControllersRouter : public trantor::NonCopyable
{
  public:
    HttpControllersRouter(const std::deque<std::function<void(const HttpRequestPtr &,
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
        : _postRoutingAdvices(postRoutingAdvices),
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
                     const std::vector<std::string> &filters);
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               bool needSetJsessionid,
               std::string &&sessionId);

  private:
    struct CtrlBinder
    {
        internal::HttpBinderBasePtr _binderPtr;
        std::vector<std::string> _filterNames;
        std::vector<std::shared_ptr<HttpFilterBase>> _filters;
        std::vector<size_t> _parameterPlaces;
        std::map<std::string, size_t> _queryParametersPlaces;
        std::map<trantor::EventLoop *, std::shared_ptr<HttpResponse>> _responsePtrMap;
        bool _isCORS = false;
    };
    typedef std::shared_ptr<CtrlBinder> CtrlBinderPtr;
    struct HttpControllerRouterItem
    {
        std::string _pathParameterPattern;
        std::string _pathPattern;
        std::regex _regex;
        CtrlBinderPtr _binders[Invalid] = {nullptr}; //The enum value Invalid is the http methods number
    };
    std::vector<HttpControllerRouterItem> _ctrlVector;
    std::mutex _ctrlMutex;
    std::regex _ctrlRegex;

    const std::deque<std::function<void(const HttpRequestPtr &,
                                        const AdviceCallback &,
                                        const AdviceChainCallback &)>> &_postRoutingAdvices;
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         const AdviceCallback &,
                                         const AdviceChainCallback &)>> &_preHandlingAdvices;
    const std::deque<std::function<void(const HttpRequestPtr &)>>
        &_postRoutingObservers;
    const std::vector<std::function<void(const HttpRequestPtr &)>>
        &_preHandlingObservers;
    const std::deque<std::function<void(const HttpRequestPtr &,
                                        const HttpResponsePtr &)>>
        &_postHandlingAdvices;

    void doPreHandlingAdvices(const CtrlBinderPtr &ctrlBinderPtr,
                              const HttpControllerRouterItem &routerItem,
                              const HttpRequestImplPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback,
                              bool needSetJsessionid,
                              std::string &&sessionId);

    void doControllerHandler(const CtrlBinderPtr &ctrlBinderPtr,
                             const HttpControllerRouterItem &routerItem,
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
            advice(req, resp);
        }
        callback(resp);
    }
};
} // namespace drogon