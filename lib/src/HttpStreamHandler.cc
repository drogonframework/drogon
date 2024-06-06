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
#include <drogon/HttpStreamHandler.h>

#include <utility>

namespace drogon
{

/**
 * A default implementation for convenience
 */
class DefaultStreamHandler : public HttpStreamHandler
{
  public:
    DefaultStreamHandler(StreamDataCallback dataCb,
                         StreamFinishCallback finishCb)
        : dataCb_(std::move(dataCb)), finishCb_(std::move(finishCb))
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

  private:
    StreamDataCallback dataCb_;
    StreamFinishCallback finishCb_;
};

HttpStreamHandlerPtr HttpStreamHandler::newHandler(
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb)
{
    return std::make_shared<DefaultStreamHandler>(std::move(dataCb),
                                                  std::move(finishCb));
}

}  // namespace drogon
