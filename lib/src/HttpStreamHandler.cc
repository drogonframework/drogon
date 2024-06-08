/**
 *
 *  @file HttpStreamHandler.cc
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

#include "MultipartStreamParser.h"
#include "drogon/HttpRequest.h"
#include <drogon/HttpStreamHandler.h>
#include <variant>

namespace drogon
{
/**
 * A default implementation for convenience
 */
class DefaultStreamHandler : public HttpStreamHandler
{
  public:
    DefaultStreamHandler(StreamDataCallback dataCb,
                         StreamFinishCallback finishCb,
                         StreamErrorCallback errorCb)
        : dataCb_(std::move(dataCb)),
          finishCb_(std::move(finishCb)),
          errorCb_(std::move(errorCb))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        dataCb_(data, length);
    }

    void onStreamFinish() override
    {
        finishCb_();
    }

    void onStreamError(std::exception_ptr ex) override
    {
        errorCb_(std::move(ex));
    }

  private:
    StreamDataCallback dataCb_;
    StreamFinishCallback finishCb_;
    StreamErrorCallback errorCb_;
};

/**
 * Parse multipart data and return actual content
 */
class MultipartStreamHandler : public HttpStreamHandler
{
  public:
    MultipartStreamHandler(const std::string &contentType,
                           MultipartHeaderCallback headerCb,
                           StreamDataCallback dataCb,
                           StreamFinishCallback finishCb,
                           StreamErrorCallback errorCb)
        : parser_(contentType),
          headerCb_(std::move(headerCb)),
          dataCb_(std::move(dataCb)),
          finishCb_(std::move(finishCb)),
          errorCb_(std::move(errorCb))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        if (!parser_.isValid() || parser_.isFinished())
        {
            return;
        }
        parser_.parse(data, length, headerCb_, dataCb_);
        if (!parser_.isValid())
        {
            errorCb_(std::make_exception_ptr(
                std::runtime_error("invalid multipart data")));
        }
        else if (parser_.isFinished())
        {
            finishCb_();
        }
    }

    void onStreamFinish() override
    {
        if (!parser_.isFinished())
        {
            finishCb_();
        }
    }

    void onStreamError(std::exception_ptr ex) override
    {
        if (parser_.isValid())
        {
            errorCb_(ex);
        }
    }

  private:
    MultipartStreamParser parser_;
    MultipartHeaderCallback headerCb_;
    StreamDataCallback dataCb_;
    StreamFinishCallback finishCb_;
    StreamErrorCallback errorCb_;
};

HttpStreamHandlerPtr HttpStreamHandler::newHandler(
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb,
    StreamErrorCallback errorCb)
{
    return std::make_shared<DefaultStreamHandler>(std::move(dataCb),
                                                  std::move(finishCb),
                                                  std::move(errorCb));
}

HttpStreamHandlerPtr HttpStreamHandler::newMultipartHandler(
    const HttpRequestPtr &req,
    MultipartHeaderCallback headerCb,
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb,
    StreamErrorCallback errorCb)
{
    return std::make_shared<MultipartStreamHandler>(req->getHeader(
                                                        "content-type"),
                                                    std::move(headerCb),
                                                    std::move(dataCb),
                                                    std::move(finishCb),
                                                    std::move(errorCb));
}
}  // namespace drogon
