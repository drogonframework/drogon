#pragma once

#include <string>
#include <vector>
#include <memory>
#include <drogon/IOThreadStorage.h>
#include <drogon/HttpResponse.h>
#include "HttpRequestImpl.h"

namespace drogon
{
class HttpFilterBase;

struct ControllerBinderBase
{
    std::string handlerName_;
    std::vector<std::string> filterNames_;
    std::vector<std::shared_ptr<HttpFilterBase>> filters_;
    IOThreadStorage<HttpResponsePtr> responseCache_;
    std::shared_ptr<std::string> corsMethods_;
    bool isCORS_{false};

    virtual ~ControllerBinderBase() = default;
    virtual void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::function<
            void(const HttpRequestImplPtr &req,
                 const HttpResponsePtr &resp,
                 const std::function<void(const HttpResponsePtr &)> &callback)>
            &postHandlingCallback) = 0;
};

struct RouteResult
{
    bool found;
    std::shared_ptr<ControllerBinderBase> binderPtr;
};

}  // namespace drogon
