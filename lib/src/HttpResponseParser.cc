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
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string_view>

using namespace trantor;
using namespace drogon;

void HttpResponseParser::reset()
{
    status_ = HttpResponseParseStatus::kExpectResponseLine;
    responsePtr_.reset(new HttpResponseImpl);
    parseResponseForHeadMethod_ = false;
    leftBodyLength_ = 0;
    currentChunkLength_ = 0;
    headerSize_ = 0;
}

HttpResponseParser::HttpResponseParser(const trantor::TcpConnectionPtr &connPtr)
    : status_(HttpResponseParseStatus::kExpectResponseLine),
      responsePtr_(new HttpResponseImpl),
      conn_(connPtr)
{
}

bool HttpResponseParser::processResponseLine(const char *begin, const char *end)
{
    std::string_view remain(begin, static_cast<size_t>(end - begin));
    const auto versionEnd = remain.find(' ');
    if (versionEnd == std::string_view::npos)
        return false;

    const auto version = remain.substr(0, versionEnd);
    if (version == "HTTP/1.1")
        responsePtr_->setVersion(Version::kHttp11);
    else if (version == "HTTP/1.0")
        responsePtr_->setVersion(Version::kHttp10);
    else
        return false;

    remain.remove_prefix(versionEnd + 1);
    // RFC 9112 requires a SP after the status code even when the reason
    // phrase is empty, but servers commonly send "HTTP/1.1 200" with no
    // trailing space. We deliberately accept that form for interoperability.
    if (remain.size() < 3 || (remain.size() > 3 && remain[3] != ' '))
        return false;

    const auto statusCode = remain.substr(0, 3);
    if (!std::all_of(statusCode.begin(), statusCode.end(), [](char c) {
            return c >= '0' && c <= '9';
        }))
        return false;

    const int code = (statusCode[0] - '0') * 100 + (statusCode[1] - '0') * 10 +
                     (statusCode[2] - '0');
    responsePtr_->setStatusCode(static_cast<HttpStatusCode>(code));
    return true;
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
            if (maxHeaderSize_ != 0)
            {
                const size_t lineSize =
                    crlf ? crlf - buf->peek() + 2 : buf->readableBytes();
                if (lineSize > maxHeaderSize_ - headerSize_)
                    return false;
                if (crlf)
                    headerSize_ += lineSize;
            }
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
            if (maxHeaderSize_ != 0)
            {
                const size_t lineSize =
                    crlf ? crlf - buf->peek() + 2 : buf->readableBytes();
                if (lineSize > maxHeaderSize_ - headerSize_)
                    return false;
                if (crlf)
                    headerSize_ += lineSize;
            }
            if (crlf)
            {
                const std::string_view line(buf->peek(), crlf - buf->peek());
                if (!responsePtr_->addHeader(line))
                {
                    const std::string &len =
                        responsePtr_->getHeaderBy("content-length");
                    // LOG_INFO << "content len=" << len;
                    if (!len.empty())
                    {
                        // std::stoull accepts a sign and wraps "-1" to the
                        // maximum value; Content-Length must start with a
                        // digit.
                        if (len[0] < '0' || len[0] > '9')
                            return false;
                        try
                        {
                            leftBodyLength_ =
                                static_cast<size_t>(std::stoull(len));
                        }
                        catch (...)
                        {
                            // Malformed Content-Length from peer.
                            return false;
                        }
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
                std::string len(buf->peek(), crlf - buf->peek());
                errno = 0;
                char *end = nullptr;
                unsigned long long parsed =
                    std::strtoull(len.c_str(), &end, 16);
                if (errno == ERANGE ||
                    parsed > (std::numeric_limits<size_t>::max)() - 2 ||
                    end == len.c_str() || (*end != '\0' && *end != ';'))
                {
                    return false;
                }
                currentChunkLength_ = static_cast<size_t>(parsed);
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
