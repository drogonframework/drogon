/**
 *
 *  drogon_callbacks.h
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

#include <drogon/HttpTypes.h>
#include <functional>
#include <memory>

namespace drogon
{
class HttpResponse;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;
using AdviceCallback = std::function<void(const HttpResponsePtr &)>;
using AdviceChainCallback = std::function<void()>;
using FilterCallback = std::function<void(const HttpResponsePtr &)>;
using FilterChainCallback = std::function<void()>;
using HttpReqCallback = std::function<void(ReqResult, const HttpResponsePtr &)>;
}  // namespace drogon
