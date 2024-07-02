/**
 *
 *  @file HttpControllerBinder.h
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

#include "ControllerBinderBase.h"
#include "HttpRequestImpl.h"
#include "WebSocketConnectionImpl.h"
#include <drogon/HttpBinder.h>

namespace drogon
{

class HttpSimpleControllerBinder : public ControllerBinderBase
{
  public:
    void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const override;

    std::shared_ptr<HttpSimpleControllerBase> controller_;
};

class HttpControllerBinder : public ControllerBinderBase
{
  public:
    void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const override;

    bool isStreamHandler() const override
    {
        assert(binderPtr_);
        return binderPtr_->isStreamHandler();
    }

    internal::HttpBinderBasePtr binderPtr_;
    std::vector<size_t> parameterPlaces_;
    std::vector<std::pair<std::string, size_t>> queryParametersPlaces_;
};

struct WebsocketControllerBinder : public ControllerBinderBase
{
    std::shared_ptr<WebSocketControllerBase> controller_;

    void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const override;

    void handleNewConnection(const HttpRequestImplPtr &req,
                             const WebSocketConnectionImplPtr &wsConnPtr) const;
};

}  // namespace drogon
