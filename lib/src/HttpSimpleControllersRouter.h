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
#include "HttpSimpleControllerBinder.h"
#include <drogon/drogon_callbacks.h>
#include <drogon/utils/HttpConstraint.h>
#include <drogon/IOThreadStorage.h>
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
    HttpSimpleControllersRouter() = default;

    void registerHttpSimpleController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods);
    RouteResult tryRoute(const HttpRequestImplPtr &req);
    void init(const std::vector<trantor::EventLoop *> &ioLoops);

    std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const;

    // TODO: temporarily move to public visibility, fix it before merge
    using CtrlBinderPtr = std::shared_ptr<HttpSimpleControllerBinder>;

    struct SimpleControllerRouterItem
    {
        CtrlBinderPtr binders_[Invalid];
    };

  private:
    std::unordered_map<std::string, SimpleControllerRouterItem> simpleCtrlMap_;
    std::mutex simpleCtrlMutex_;
};
}  // namespace drogon
