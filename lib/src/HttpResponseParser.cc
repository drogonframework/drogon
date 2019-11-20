/**
 *
 *  HttpResponseParser.cc
 *  An Tao
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
#include <iostream>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
using namespace trantor;
using namespace drogon;

void HttpResponseParser::reset()
{
    status_ = HttpResponseParseStatus::kExpectResponseLine;
    responsePtr_.reset(new HttpResponseImpl);
    parseResponseForHeadMethod_ = false;
}

HttpResponseParser::HttpResponseParser()
    : status_(HttpResponseParseStatus::kExpectResponseLine),
      responsePtr_(new HttpResponseImpl)
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
            responsePtr_->setVersion(kHttp11);
        }
        else if (*(space - 1) == '0')
        {
            responsePtr_->setVersion(kHttp10);
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
                        responsePtr_->leftBodyLength_ = atoi(len.c_str());
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
                                 responsePtr_->getHeaderBy("upgrade") ==
                                     "websocket"))
                            {
                                // The Websocket response may not have a
                                // content-length header.
                                status_ = HttpResponseParseStatus::kGotAll;
                                hasMore = false;
                            }
                            else
                            {
                                status_ = HttpResponseParseStatus::kExpectClose;
                                hasMore = true;
                            }
                        }
                    }
                    if (parseResponseForHeadMethod_)
                    {
                        responsePtr_->leftBodyLength_ = 0;
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
                if (responsePtr_->leftBodyLength_ == 0)
                {
                    status_ = HttpResponseParseStatus::kGotAll;
                }
                break;
            }
            if (!responsePtr_->bodyPtr_)
            {
                responsePtr_->bodyPtr_ = std::make_shared<std::string>();
            }
            if (responsePtr_->leftBodyLength_ >= buf->readableBytes())
            {
                responsePtr_->leftBodyLength_ -= buf->readableBytes();

                responsePtr_->bodyPtr_->append(
                    std::string(buf->peek(), buf->readableBytes()));
                buf->retrieveAll();
            }
            else
            {
                responsePtr_->bodyPtr_->append(
                    std::string(buf->peek(), responsePtr_->leftBodyLength_));
                buf->retrieve(responsePtr_->leftBodyLength_);
                responsePtr_->leftBodyLength_ = 0;
            }
            if (responsePtr_->leftBodyLength_ == 0)
            {
                status_ = HttpResponseParseStatus::kGotAll;
                LOG_TRACE << "post got all:len="
                          << responsePtr_->leftBodyLength_;
                // LOG_INFO<<"content:"<<request_->content_;
                LOG_TRACE << "content(END)";
                hasMore = false;
            }
        }
        else if (status_ == HttpResponseParseStatus::kExpectClose)
        {
            if (!responsePtr_->bodyPtr_)
            {
                responsePtr_->bodyPtr_ = std::make_shared<std::string>();
            }
            responsePtr_->bodyPtr_->append(
                std::string(buf->peek(), buf->readableBytes()));
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
                responsePtr_->currentChunkLength_ =
                    strtol(len.c_str(), &end, 16);
                // LOG_TRACE << "chun length : " <<
                // responsePtr_->currentChunkLength_;
                if (responsePtr_->currentChunkLength_ != 0)
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
            // len="<<responsePtr_->currentChunkLength_;
            if (buf->readableBytes() >= (responsePtr_->currentChunkLength_ + 2))
            {
                if (*(buf->peek() + responsePtr_->currentChunkLength_) ==
                        '\r' &&
                    *(buf->peek() + responsePtr_->currentChunkLength_ + 1) ==
                        '\n')
                {
                    if (!responsePtr_->bodyPtr_)
                    {
                        responsePtr_->bodyPtr_ =
                            std::make_shared<std::string>();
                    }
                    responsePtr_->bodyPtr_->append(
                        std::string(buf->peek(),
                                    responsePtr_->currentChunkLength_));
                    buf->retrieve(responsePtr_->currentChunkLength_ + 2);
                    responsePtr_->currentChunkLength_ = 0;
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
