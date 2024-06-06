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
class HttpStreamHandler;
using HttpStreamHandlerPtr = std::shared_ptr<HttpStreamHandler>;

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

    using StreamDataCallback = std::function<void(const char *, size_t)>;
    using StreamFinishCallback = std::function<void()>;

    // Create a handler with default implementation
    static HttpStreamHandlerPtr newHandler(StreamDataCallback dataCb,
                                           StreamFinishCallback finishCb);
};

}  // namespace drogon
