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
#include <variant>
#include <deque>

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

class SomeMagicalParser
{
  public:
    using ParsedItem = std::variant<MultipartFormData, std::string>;

    void readMore(const char *data, size_t length)
    {
        // TODO
    }

    bool hasContent() const
    {
        return !parsedItems_.empty();
    }

    ParsedItem popContent()
    {
        auto item = std::move(parsedItems_.front());
        parsedItems_.pop_front();
        return item;
    }

  private:
    std::deque<ParsedItem> parsedItems_;
};

/**
 * Parse multipart data and return actual content
 */
class MultipartStreamHandler : public HttpStreamHandler
{
  public:
    MultipartStreamHandler(MultipartHeaderCallback headerCb,
                           MultipartBodyCallback bodyCb)
        : headerCb_(std::move(headerCb)), bodyCb_(std::move(bodyCb))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        parser_.readMore(data, length);
        while (parser_.hasContent())
        {
            auto content = parser_.popContent();
            if (std::holds_alternative<MultipartFormData>(content))
            {
                headerCb_(std::move(std::get<MultipartFormData>(content)));
            }
            else
            {
                bodyCb_(std::move(std::get<std::string>(content)));
            }
        }
    }

    void onStreamFinish() override
    {
        bodyCb_({});
    }

  private:
    SomeMagicalParser parser_;
    MultipartHeaderCallback headerCb_;
    MultipartBodyCallback bodyCb_;
};

HttpStreamHandlerPtr HttpStreamHandler::newHandler(
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb)
{
    return std::make_shared<DefaultStreamHandler>(std::move(dataCb),
                                                  std::move(finishCb));
}

HttpStreamHandlerPtr HttpStreamHandler::newMultipartHandler(
    HttpStreamHandler::MultipartHeaderCallback headerCb,
    HttpStreamHandler::MultipartBodyCallback bodyCb)
{
    return std::make_shared<MultipartStreamHandler>(std::move(headerCb),
                                                    std::move(bodyCb));
}

}  // namespace drogon
