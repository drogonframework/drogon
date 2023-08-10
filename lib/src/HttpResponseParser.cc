/**
 *
 *  @file HttpResponseParser.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "HttpResponseParser.h"
#include "HttpResponseImpl.h"
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <algorithm>

using namespace trantor;
using namespace drogon;

void HttpResponseParser::reset()
{
    status_ = HttpResponseParseStatus::kExpectResponseLine;
    responsePtr_.reset(new HttpResponseImpl);
    parseResponseForHeadMethod_ = false;
    leftBodyLength_ = 0;
    currentChunkLength_ = 0;
}

HttpResponseParser::HttpResponseParser(const trantor::TcpConnectionPtr &connPtr)
    : status_(HttpResponseParseStatus::kExpectResponseLine),
      responsePtr_(new HttpResponseImpl),
      conn_(connPtr)
{
}

bool HttpResponseParser::processResponseLine(const char *begin, const char *end)
{
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end)
    {
        LOG_TRACE << *(space - 1);
        if (*(space - 1) == '1')
        {
            responsePtr_->setVersion(Version::kHttp11);
        }
        else if (*(space - 1) == '0')
        {
            responsePtr_->setVersion(Version::kHttp10);
        }
        else
        {
            return false;
        }
    }

    start = space + 1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
        std::string status_code(start, space - start);
        std::string status_message(space + 1, end - space - 1);
        LOG_TRACE << status_code << " " << status_message;
        auto code = atoi(status_code.c_str());
        responsePtr_->setStatusCode(HttpStatusCode(code));

        return true;
    }
    return false;
}

bool HttpResponseParser::parseResponseOnClose()
{
    if (status_ == HttpResponseParseStatus::kExpectClose)
    {
        status_ = HttpResponseParseStatus::kGotAll;
        return true;
    }
    return false;
}

// return false if any error
bool HttpResponseParser::parseResponse(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        if (status_ == HttpResponseParseStatus::kExpectResponseLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processResponseLine(buf->peek(), crlf);
                if (ok)
                {
                    // responsePtr_->setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    status_ = HttpResponseParseStatus::kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    responsePtr_->addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    const std::string &len =
                        responsePtr_->getHeaderBy("content-length");
                    // LOG_INFO << "content len=" << len;
                    if (!len.empty())
                    {
                        leftBodyLength_ = static_cast<size_t>(std::stoull(len));
                        status_ = HttpResponseParseStatus::kExpectBody;
                    }
                    else
                    {
                        const std::string &encode =
                            responsePtr_->getHeaderBy("transfer-encoding");
                        if (encode == "chunked")
                        {
                            status_ = HttpResponseParseStatus::kExpectChunkLen;
                            hasMore = true;
                        }
                        else
                        {
                            if (responsePtr_->statusCode() == k204NoContent ||
                                (responsePtr_->statusCode() ==
                                         k101SwitchingProtocols &&
                                     [this]() -> bool {
                                    std::string upgradeValue =
                                        responsePtr_->getHeaderBy("upgrade");
                                    std::transform(upgradeValue.begin(),
                                                   upgradeValue.end(),
                                                   upgradeValue.begin(),
                                                   [](unsigned char c) {
                                                       return tolower(c);
                                                   });
                                    return upgradeValue == "websocket";
                                }()))
                            {
                                // The Websocket response may not have a
                                // content-length header.
                                status_ = HttpResponseParseStatus::kGotAll;
                                hasMore = false;
                            }
                            else
                            {
                                status_ = HttpResponseParseStatus::kExpectClose;
                                auto connPtr = conn_.lock();
                                connPtr->shutdown();
                                hasMore = true;
                            }
                        }
                    }
                    if (parseResponseForHeadMethod_)
                    {
                        leftBodyLength_ = 0;
                        status_ = HttpResponseParseStatus::kGotAll;
                        hasMore = false;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectBody)
        {
            // LOG_INFO << "expectBody:len=" << request_->contentLen;
            // LOG_INFO << "expectBody:buf=" << buf;
            if (buf->readableBytes() == 0)
            {
                if (leftBodyLength_ == 0)
                {
                    status_ = HttpResponseParseStatus::kGotAll;
                }
                break;
            }
            if (!responsePtr_->bodyPtr_)
            {
                responsePtr_->bodyPtr_ =
                    std::make_shared<HttpMessageStringBody>();
            }
            if (leftBodyLength_ >= buf->readableBytes())
            {
                leftBodyLength_ -= buf->readableBytes();

                responsePtr_->bodyPtr_->append(buf->peek(),
                                               buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                responsePtr_->bodyPtr_->append(buf->peek(), leftBodyLength_);
                buf->retrieve(leftBodyLength_);
                leftBodyLength_ = 0;
            }
            if (leftBodyLength_ == 0)
            {
                status_ = HttpResponseParseStatus::kGotAll;
                LOG_TRACE << "post got all:len=" << leftBodyLength_;
                // LOG_INFO<<"content:"<<request_->content_;
                LOG_TRACE << "content(END)";
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectClose)
        {
            if (!responsePtr_->bodyPtr_)
            {
                responsePtr_->bodyPtr_ =
                    std::make_shared<HttpMessageStringBody>();
            }
            responsePtr_->bodyPtr_->append(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
            break;
        }
        else if (status_ == HttpResponseParseStatus::kExpectChunkLen)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                // chunk length line
                std::string len(buf->peek(), crlf - buf->peek());
                char *end;
                currentChunkLength_ = strtol(len.c_str(), &end, 16);
                // LOG_TRACE << "chun length : " <<
                // currentChunkLength_;
                if (currentChunkLength_ != 0)
                {
                    status_ = HttpResponseParseStatus::kExpectChunkBody;
                }
                else
                {
                    status_ = HttpResponseParseStatus::kExpectLastEmptyChunk;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectChunkBody)
        {
            // LOG_TRACE<<"expect chunk
            // len="<<currentChunkLength_;
            if (buf->readableBytes() >= (currentChunkLength_ + 2))
            {
                if (*(buf->peek() + currentChunkLength_) == '\r' &&
                    *(buf->peek() + currentChunkLength_ + 1) == '\n')
                {
                    if (!responsePtr_->bodyPtr_)
                    {
                        responsePtr_->bodyPtr_ =
                            std::make_shared<HttpMessageStringBody>();
                    }
                    responsePtr_->bodyPtr_->append(buf->peek(),
                                                   currentChunkLength_);
                    buf->retrieve(currentChunkLength_ + 2);
                    currentChunkLength_ = 0;
                    status_ = HttpResponseParseStatus::kExpectChunkLen;
                }
                else
                {
                    // error!
                    buf->retrieveAll();
                    return false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectLastEmptyChunk)
        {
            // last empty chunk
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                buf->retrieveUntil(crlf + 2);
                status_ = HttpResponseParseStatus::kGotAll;
                responsePtr_->addHeader("content-length",
                                        std::to_string(
                                            responsePtr_->getBody().length()));
                responsePtr_->removeHeaderBy("transfer-encoding");
                break;
            }
            else
            {
                hasMore = false;
            }
        }
    }
    return ok;
}
