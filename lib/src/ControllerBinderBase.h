/**
 *
 *  @file ControllerBinderBase.h
 *  @author Nitromelon
 *
 *  Copyright 2023, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <drogon/IOThreadStorage.h>
#include <drogon/HttpResponse.h>
#include "HttpRequestImpl.h"

namespace drogon
{
class HttpMiddlewareBase;

/**
 * @brief A component to associate router class and controller class
 */
struct ControllerBinderBase
{
    std::string handlerName_;
    std::vector<std::string> middlewareNames_;
    std::vector<std::shared_ptr<HttpMiddlewareBase>> middlewares_;
    IOThreadStorage<HttpResponsePtr> responseCache_;
    std::shared_ptr<std::string> corsMethods_;
    bool isCORS_{false};

    virtual ~ControllerBinderBase() = default;
    virtual void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const = 0;

    virtual bool isStreamHandler() const
    {
        return false;
    }
};

struct RouteResult
{
    enum
    {
        Success,
        MethodNotAllowed,
        NotFound
    } result;

    std::shared_ptr<ControllerBinderBase> binderPtr;
};

}  // namespace drogon
