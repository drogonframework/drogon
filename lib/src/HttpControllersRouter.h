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
#include "ControllerBinderBase.h"
#include <trantor/utils/NonCopyable.h>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include <unordered_map>

namespace drogon
{
class HttpControllerBinder;
class HttpSimpleControllerBinder;
struct WebsocketControllerBinder;

class HttpControllersRouter : public trantor::NonCopyable
{
  public:
    static HttpControllersRouter &instance()
    {
        static HttpControllersRouter inst;
        return inst;
    }

    void init(const std::vector<trantor::EventLoop *> &ioLoops);
    // clean all resources
    void reset();

    void registerHttpSimpleController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &constraints);
    void registerWebSocketController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &constraints);
    void registerWebSocketControllerRegex(
        const std::string &regExp,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &constraints);
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &middlewareNames,
                     const std::string &handlerName = "");
    void addHttpRegex(const std::string &regExp,
                      const internal::HttpBinderBasePtr &binder,
                      const std::vector<HttpMethod> &validMethods,
                      const std::vector<std::string> &middlewareNames,
                      const std::string &handlerName = "");
    RouteResult route(const HttpRequestImplPtr &req);
    RouteResult routeWs(const HttpRequestImplPtr &req);
    std::vector<HttpHandlerInfo> getHandlersInfo() const;

  private:
    void addRegexCtrlBinder(
        const std::shared_ptr<HttpControllerBinder> &binderPtr,
        const std::string &pathPattern,
        const std::string &pathParameterPattern,
        const std::vector<HttpMethod> &methods);

    struct SimpleControllerRouterItem
    {
        std::shared_ptr<HttpSimpleControllerBinder> binders_[Invalid]{nullptr};
    };

    struct HttpControllerRouterItem
    {
        std::string pathParameterPattern_;
        std::string pathPattern_;
        std::regex regex_;
        std::shared_ptr<HttpControllerBinder> binders_[Invalid]{nullptr};
    };

    struct WebSocketControllerRouterItem
    {
        std::shared_ptr<WebsocketControllerBinder> binders_[Invalid]{nullptr};
    };

    struct RegExWebSocketControllerRouterItem
    {
        std::string pathPattern_;
        std::regex regex_;
        std::shared_ptr<WebsocketControllerBinder> binders_[Invalid]{nullptr};
    };

    std::unordered_map<std::string, SimpleControllerRouterItem> simpleCtrlMap_;
    std::unordered_map<std::string, HttpControllerRouterItem> ctrlMap_;
    std::vector<HttpControllerRouterItem> ctrlVector_;  // for regexp path
    std::unordered_map<std::string, WebSocketControllerRouterItem> wsCtrlMap_;
    std::vector<RegExWebSocketControllerRouterItem> wsCtrlVector_;
};
}  // namespace drogon
