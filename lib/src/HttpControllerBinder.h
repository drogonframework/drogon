#pragma once

#include "ControllerBinderBase.h"
#include "HttpRequestImpl.h"
#include <drogon/HttpBinder.h>

namespace drogon
{

class HttpControllerBinder : public ControllerBinderBase
{
  public:
    void handleRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::function<
            void(const HttpRequestImplPtr &req,
                 const HttpResponsePtr &resp,
                 const std::function<void(const HttpResponsePtr &)> &callback)>
            &postHandlingCallback) override;

    internal::HttpBinderBasePtr binderPtr_;
    std::vector<size_t> parameterPlaces_;
    std::vector<std::pair<std::string, size_t>> queryParametersPlaces_;
};

}  // namespace drogon
