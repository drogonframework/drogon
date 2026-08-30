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
#include <exception>

namespace drogon
{
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

class RequestStreamReader;
using RequestStreamReaderPtr = std::shared_ptr<RequestStreamReader>;

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
    virtual void setStreamReader(RequestStreamReaderPtr reader) = 0;
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
 * An interface for stream request reading.
 * User should create an implementation class, or use built-in handlers
 */
class DROGON_EXPORT RequestStreamReader
{
  public:
    virtual ~RequestStreamReader() = default;
    virtual void onStreamData(const char *, size_t) = 0;
    virtual void onStreamFinish(std::exception_ptr) = 0;

    using StreamDataCallback = std::function<void(const char *, size_t)>;
    using StreamFinishCallback = std::function<void(std::exception_ptr)>;

    // Create a handler with default implementation
    static RequestStreamReaderPtr newReader(StreamDataCallback dataCb,
                                            StreamFinishCallback finishCb);

    // A handler that drops all data
    static RequestStreamReaderPtr newNullReader();

    using MultipartHeaderCallback = std::function<void(MultipartHeader header)>;

    static RequestStreamReaderPtr newMultipartReader(
        const HttpRequestPtr &req,
        MultipartHeaderCallback headerCb,
        StreamDataCallback dataCb,
        StreamFinishCallback finishCb);
};

}  // namespace drogon
