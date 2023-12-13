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
