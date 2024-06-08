/**
 *
 *  @file HttpStreamHandler.h
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
#include <memory>
#include <functional>

namespace drogon
{
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

class HttpStreamHandler;
using HttpStreamHandlerPtr = std::shared_ptr<HttpStreamHandler>;

struct MultipartHeader
{
    std::string name;
    std::string filename;
    std::string contentType;
};

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
 * User should create an implementation class.
 */
class HttpStreamHandler
{
  public:
    virtual ~HttpStreamHandler() = default;
    virtual void onStreamData(const char *, size_t) = 0;
    virtual void onStreamFinish() = 0;
    virtual void onStreamError(std::exception_ptr) = 0;

    using StreamDataCallback = std::function<void(const char *, size_t)>;
    using StreamFinishCallback = std::function<void()>;
    using StreamErrorCallback = std::function<void(std::exception_ptr)>;

    // Create a handler with default implementation
    static HttpStreamHandlerPtr newHandler(StreamDataCallback dataCb,
                                           StreamFinishCallback finishCb,
                                           StreamErrorCallback errorCb);

    using MultipartHeaderCallback = std::function<void(MultipartHeader header)>;

    static HttpStreamHandlerPtr newMultipartHandler(
        const HttpRequestPtr &req,
        MultipartHeaderCallback headerCb,
        StreamDataCallback dataCb,
        StreamFinishCallback finishCb,
        StreamErrorCallback errorCb);
};

}  // namespace drogon
