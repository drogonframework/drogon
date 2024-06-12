/**
 *
 *  @file RequestStream.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once
#include <drogon/exports.h>
#include <memory>
#include <functional>

namespace drogon
{
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

class RequestStreamHandler;
using RequestStreamHandlerPtr = std::shared_ptr<RequestStreamHandler>;

struct MultipartHeader
{
    std::string name;
    std::string filename;
    std::string contentType;
};

class DROGON_EXPORT RequestStream
{
  public:
    virtual void setStreamHandler(RequestStreamHandlerPtr handler) = 0;
};

using RequestStreamPtr = std::shared_ptr<RequestStream>;

namespace internal
{
DROGON_EXPORT RequestStreamPtr createRequestStream(const HttpRequestPtr &req);
}

/**
 * TODO-s:
 * - Too many callbacks
 * - Is `finishCb` necessary? Calling `dataCb` with null can be a replacement.
 * - Should we notify user on each multipart block finish?
 * - Is `errorCb` necessary? How does it act? Should user or framework send the
 *   error response?
 */

/**
 * An interface for stream request handling.
 * User should create an implementation class, or use built-in handlers
 */
class RequestStreamHandler
{
  public:
    virtual ~RequestStreamHandler() = default;
    virtual void onStreamData(const char *, size_t) = 0;
    virtual void onStreamFinish() = 0;
    virtual void onStreamError(std::exception_ptr) = 0;

    using StreamDataCallback = std::function<void(const char *, size_t)>;
    using StreamFinishCallback = std::function<void()>;
    using StreamErrorCallback = std::function<void(std::exception_ptr)>;

    // Create a handler with default implementation
    static RequestStreamHandlerPtr newHandler(StreamDataCallback dataCb,
                                              StreamFinishCallback finishCb,
                                              StreamErrorCallback errorCb);

    using MultipartHeaderCallback = std::function<void(MultipartHeader header)>;

    static RequestStreamHandlerPtr newMultipartHandler(
        const HttpRequestPtr &req,
        MultipartHeaderCallback headerCb,
        StreamDataCallback dataCb,
        StreamFinishCallback finishCb,
        StreamErrorCallback errorCb);
};

}  // namespace drogon
