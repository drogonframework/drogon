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
typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
typedef std::function<void(const HttpResponsePtr &)> AdviceCallback;
typedef std::function<void()> AdviceChainCallback;
typedef std::function<void(const HttpResponsePtr &)> FilterCallback;
typedef std::function<void()> FilterChainCallback;
typedef std::function<void(ReqResult, const HttpResponsePtr &)> HttpReqCallback;
}  // namespace drogon
