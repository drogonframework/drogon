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
#include "HttpUtils.h"
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>

using namespace trantor;
using namespace drogon;

static bool lastTransferCodingIsChunked(const std::string &value)
{
    // Transfer-Encoding is a comma-separated list of codings; the final one
    // determines the framing. Coding names are case-insensitive tokens.
    const auto lastEnd = value.find_last_not_of(" \t");
    if (lastEnd == std::string::npos)
        return false;
    const auto comma = value.rfind(',', lastEnd);
    auto begin = (comma == std::string::npos) ? 0 : comma + 1;
    while (begin <= lastEnd && (value[begin] == ' ' || value[begin] == '\t'))
        ++begin;
    std::string coding(value, begin, lastEnd - begin + 1);
    // Drop any transfer parameters, e.g. "chunked;foo=bar".
    const auto semicolon = coding.find(';');
    if (semicolon != std::string::npos)
        coding.resize(semicolon);
    while (!coding.empty() && (coding.back() == ' ' || coding.back() == '\t'))
        coding.pop_back();
    std::transform(coding.begin(),
                   coding.end(),
                   coding.begin(),
                   [](unsigned char c) { return tolower(c); });
    return coding == "chunked";
}

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
                    if (!responsePtr_->addHeader(buf->peek(), colon, crlf))
                    {
                        // Invalid header section, e.g. a repeated
                        // Content-Length field.
                        return false;
                    }
                }
                else
                {
                    // End of the header section. Decide how the body is
                    // delimited (RFC 9112 section 6.3).
                    const auto code =
                        static_cast<int>(responsePtr_->statusCode());
                    const auto &headers = responsePtr_->headers();
                    const auto lenIt = headers.find("content-length");
                    const auto encodeIt = headers.find("transfer-encoding");
                    const bool hasLen = lenIt != headers.end();
                    const bool hasEncode =
                        encodeIt != headers.end() && !encodeIt->second.empty();

                    if (parseResponseForHeadMethod_)
                    {
                        // A response to HEAD never has a body, regardless of
                        // the framing fields it carries.
                        leftBodyLength_ = 0;
                        status_ = HttpResponseParseStatus::kGotAll;
                        hasMore = false;
                    }
                    else if (code == k204NoContent || code == k304NotModified ||
                             (code >= 100 && code < 200 &&
                              code != k101SwitchingProtocols))
                    {
                        // RFC 9112 section 6.3: these responses are always
                        // terminated by the empty line after the header
                        // section. A Content-Length is allowed on 304 but
                        // does not describe a body. Informational responses
                        // are skipped by the caller.
                        status_ = HttpResponseParseStatus::kGotAll;
                        hasMore = false;
                    }
                    else if (code == k101SwitchingProtocols)
                    {
                        std::string upgradeValue =
                            responsePtr_->getHeaderBy("upgrade");
                        std::transform(upgradeValue.begin(),
                                       upgradeValue.end(),
                                       upgradeValue.begin(),
                                       [](unsigned char c) {
                                           return tolower(c);
                                       });
                        if (upgradeValue == "websocket")
                        {
                            // The Websocket response may not have a
                            // content-length header.
                            status_ = HttpResponseParseStatus::kGotAll;
                            hasMore = false;
                        }
                        else
                        {
                            status_ = HttpResponseParseStatus::kExpectClose;
                            if (auto connPtr = conn_.lock())
                                connPtr->shutdown();
                        }
                    }
                    else if (hasLen && hasEncode)
                    {
                        // RFC 9112 section 6.3: Transfer-Encoding and
                        // Content-Length together make the framing ambiguous,
                        // which is a response smuggling vector. Reject rather
                        // than trust one of them.
                        return false;
                    }
                    else if (hasEncode)
                    {
                        if (lastTransferCodingIsChunked(encodeIt->second))
                        {
                            status_ = HttpResponseParseStatus::kExpectChunkLen;
                        }
                        else
                        {
                            // Unknown transfer coding: the body runs until
                            // the connection is closed.
                            status_ = HttpResponseParseStatus::kExpectClose;
                            if (auto connPtr = conn_.lock())
                                connPtr->shutdown();
                        }
                    }
                    else if (hasLen)
                    {
                        if (!parseContentLength(lenIt->second, leftBodyLength_))
                        {
                            // Malformed Content-Length from peer.
                            return false;
                        }
                        status_ = HttpResponseParseStatus::kExpectBody;
                    }
                    else
                    {
                        status_ = HttpResponseParseStatus::kExpectClose;
                        if (auto connPtr = conn_.lock())
                            connPtr->shutdown();
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
            // The last chunk may be followed by a trailer section that is
            // always terminated by an empty line (RFC 9112 section 7.1).
            // Skip it line by line; consuming a single line would leave the
            // rest in the buffer to be parsed as the next response.
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const bool emptyLine = (crlf == buf->peek());
                buf->retrieveUntil(crlf + 2);
                if (emptyLine)
                {
                    status_ = HttpResponseParseStatus::kGotAll;
                    responsePtr_->addHeader(
                        "content-length",
                        std::to_string(responsePtr_->getBody().length()));
                    responsePtr_->removeHeaderBy("transfer-encoding");
                    break;
                }
            }
            else
            {
                hasMore = false;
            }
        }
    }
    return ok;
}
