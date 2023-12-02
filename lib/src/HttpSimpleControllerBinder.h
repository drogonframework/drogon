#pragma once

#include "ControllerBinderBase.h"
#include "HttpRequestImpl.h"

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

}  // namespace drogon
