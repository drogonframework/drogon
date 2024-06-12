/**
 *
 *  @file RequestStreamHandler.cc
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
#include "HttpRequestImpl.h"

#include <drogon/RequestStream.h>
#include <variant>

namespace drogon
{
/**
 * A default implementation for convenience
 */
class DefaultStreamHandler : public RequestStreamHandler
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
class MultipartStreamHandler : public RequestStreamHandler
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

RequestStreamHandlerPtr RequestStreamHandler::newHandler(
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb,
    StreamErrorCallback errorCb)
{
    return std::make_shared<DefaultStreamHandler>(std::move(dataCb),
                                                  std::move(finishCb),
                                                  std::move(errorCb));
}

RequestStreamHandlerPtr RequestStreamHandler::newMultipartHandler(
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

class RequestStreamImpl : public RequestStream
{
  public:
    RequestStreamImpl(const HttpRequestImplPtr &req) : weakReq_(req)
    {
    }

    void setStreamHandler(RequestStreamHandlerPtr handler) override
    {
        if (auto req = weakReq_.lock())
        {
            auto loop = req->getLoop();
            if (loop->isInLoopThread())
            {
                req->setStreamHandler(std::move(handler));
            }
            else
            {
                loop->queueInLoop([req = std::move(req),
                                   handler = std::move(handler)]() mutable {
                    req->setStreamHandler(std::move(handler));
                });
            }
        }
    }

  private:
    std::weak_ptr<HttpRequestImpl> weakReq_;
};

namespace internal
{
RequestStreamPtr createRequestStream(const HttpRequestPtr &req)
{
    auto reqImpl = std::static_pointer_cast<HttpRequestImpl>(req);
    if (!reqImpl->isStreamMode())
    {
        return nullptr;
    }
    return std::make_shared<RequestStreamImpl>(
        std::static_pointer_cast<HttpRequestImpl>(req));
}
}  // namespace internal

}  // namespace drogon
