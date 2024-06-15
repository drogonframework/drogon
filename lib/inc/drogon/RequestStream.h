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
#include <string>
#include <functional>
#include <memory>

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
    virtual ~RequestStream() = default;
    virtual void setStreamHandler(RequestStreamHandlerPtr handler) = 0;
};

using RequestStreamPtr = std::shared_ptr<RequestStream>;

namespace internal
{
DROGON_EXPORT RequestStreamPtr createRequestStream(const HttpRequestPtr &req);
}

enum class StreamErrorCode
{
    kNone = 0,
    kBadRequest,
    kConnectionBroken
};

class StreamError final : public std::exception
{
  public:
    const char *what() const noexcept override
    {
        return message_.data();
    }

    StreamErrorCode code() const
    {
        return code_;
    }

    StreamError(StreamErrorCode code, const std::string &message)
        : message_(message), code_(code)
    {
    }

    StreamError(StreamErrorCode code, std::string &&message)
        : message_(std::move(message)), code_(code)
    {
    }

    StreamError() = delete;

  private:
    std::string message_;
    StreamErrorCode code_;
};

/**
 * An interface for stream request handling.
 * User should create an implementation class, or use built-in handlers
 */
class RequestStreamHandler
{
  public:
    virtual ~RequestStreamHandler() = default;
    virtual void onStreamData(const char *, size_t) = 0;
    virtual void onStreamFinish(std::exception_ptr) = 0;

    using StreamDataCallback = std::function<void(const char *, size_t)>;
    using StreamFinishCallback = std::function<void(std::exception_ptr)>;

    // Create a handler with default implementation
    static RequestStreamHandlerPtr newHandler(StreamDataCallback dataCb,
                                              StreamFinishCallback finishCb);

    // A handler that drops all data
    static RequestStreamHandlerPtr newNullHandler();

    using MultipartHeaderCallback = std::function<void(MultipartHeader header)>;

    static RequestStreamHandlerPtr newMultipartHandler(
        const HttpRequestPtr &req,
        MultipartHeaderCallback headerCb,
        StreamDataCallback dataCb,
        StreamFinishCallback finishCb);
};

}  // namespace drogon
