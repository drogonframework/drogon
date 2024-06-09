/**
 *
 *  HttpBinder.h
 *  Martin Chang
 *
 *  Copyright 2021, Martin Chang.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/HttpBinder.h>
#include <drogon/HttpAppFramework.h>
#include "HttpRequestImpl.h"

namespace drogon
{
namespace internal
{
void handleException(const std::exception &e,
                     const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
{
    app().getExceptionHandler()(e, req, std::move(callback));
}

bool isStreamMode(const HttpRequestPtr &req)
{
    return static_cast<HttpRequestImpl *>(req.get())->isStreamMode();
}

void waitForFullBody(const HttpRequestPtr &req, std::function<void()> &&cb)
{
    auto reqImpl = static_cast<HttpRequestImpl *>(req.get());
    if (!reqImpl->isStreamMode())
    {
        cb();
        return;
    }
    reqImpl->waitForStreamFinish(std::move(cb));
}

}  // namespace internal
}  // namespace drogon
