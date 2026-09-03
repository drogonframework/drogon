/**
 *
 *  HttpRequestParser.cc
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

#include "HttpRequestParser.h"
#include <drogon/HttpTypes.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <iostream>
#include "HttpAppFrameworkImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpUtils.h"
#include <drogon/utils/Utilities.h>

using namespace trantor;
using namespace drogon;

static constexpr size_t CRLF_LEN = 2;            // strlen("crlf")
static constexpr size_t METHOD_MAX_LEN = 7;      // strlen("OPTIONS")
static constexpr size_t TRUNK_LEN_MAX_LEN = 16;  // 0xFFFFFFFF,FFFFFFFF
static constexpr size_t HEADER_LINE_MAX_LEN = 64 * 1024;
static constexpr size_t HEADER_SECTION_MAX_LEN = 1024 * 1024;

// RFC 9110 Section 5.5 permits SP, HTAB, visible US-ASCII, and obs-text in a
// field value.  All other control octets are invalid.  In particular, CR, LF,
// and NUL must be rejected or replaced before processing or forwarding; reject
// them here so the received message is never interpreted in two different ways.
static bool isValidHttpFieldValue(const char *begin, const char *end)
{
    return std::all_of(begin, end, [](unsigned char c) {
        return c == '\t' || c == ' ' || (c >= 0x21 && c <= 0x7e) || c >= 0x80;
    });
}

HttpRequestParser::HttpRequestParser(const trantor::TcpConnectionPtr &connPtr)
    : HttpRequestParser(connPtr->getLoop())
{
    conn_ = connPtr;
}

HttpRequestParser::HttpRequestParser(trantor::EventLoop *loop)
    : status_(HttpRequestParseStatus::kExpectMethod), loop_(loop)
{
}

bool HttpRequestParser::processRequestLine(const char *begin, const char *end)
{
    const char *space = std::find(begin, end, ' ');
    if (space == begin || space == end)
        return false;

    const char *version = space + 1;
    if (end - version != 8 || !std::equal(version, end - 1, "HTTP/1.") ||
        (*(end - 1) != '0' && *(end - 1) != '1'))
    {
        return false;
    }

    if (std::find_if(begin, space, [](unsigned char c) {
            return c <= 0x20 || c == 0x7f || c == '#';
        }) != space)
    {
        return false;
    }

    const char *question = std::find(begin, space, '?');
    if (question == begin)
        return false;

    const char *pathBegin = begin;
    if (question - begin == 1 && *begin == '*')
    {
        if (request_->method() != Options || question != space)
            return false;
    }
    else if (*begin != '/')
    {
        const char *scheme = std::search(begin, question, "://", "://" + 3);
        if (scheme == begin || scheme == question)
            return false;
        const char *authority = scheme + 3;
        pathBegin = std::find(authority, question, '/');
        if (authority == pathBegin)
            return false;
    }

    if (pathBegin == question)
        request_->setPath("/");
    else
        request_->setPath(pathBegin, question);
    if (question != space)
    {
        request_->setQuery(question + 1, space);
    }
    request_->setVersion(*(end - 1) == '1' ? Version::kHttp11
                                           : Version::kHttp10);
    return true;
}

HttpRequestImplPtr HttpRequestParser::makeRequestForPool(HttpRequestImpl *ptr)
{
    return std::shared_ptr<HttpRequestImpl>(
        ptr, [weakPtr = weak_from_this()](HttpRequestImpl *p) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                if (thisPtr->loop_->isInLoopThread())
                {
                    p->reset();
                    thisPtr->requestsPool_.emplace_back(
                        thisPtr->makeRequestForPool(p));
                }
                else
                {
                    auto &loop = thisPtr->loop_;
                    loop->queueInLoop([thisPtr = std::move(thisPtr), p]() {
                        p->reset();
                        thisPtr->requestsPool_.emplace_back(
                            thisPtr->makeRequestForPool(p));
                    });
                }
            }
            else
            {
                delete p;
            }
        });
}

void HttpRequestParser::reset()
{
    assert(loop_->isInLoopThread());
    remainContentLength_ = 0;
    headerBytes_ = 0;
    trailerBytes_ = 0;
    contentLengthHeaderSeen_ = false;
    transferEncodingHeaderSeen_ = false;
    hostHeaderSeen_ = false;
    status_ = HttpRequestParseStatus::kExpectMethod;
    if (requestsPool_.empty())
    {
        request_ = makeRequestForPool(new HttpRequestImpl(loop_));
    }
    else
    {
        auto req = std::move(requestsPool_.back());
        requestsPool_.pop_back();
        request_ = std::move(req);
        request_->setCreationDate(trantor::Date::now());
    }
}

/**
 * @return return -HttpStatusCode if encounters any http errors in request
 * @return return -1 if encounters any other errors in request
 * @return return 0 if request is not ready
 * @return return 1 if request is ready
 * @return return 2 if request is ready and entering stream mode
 * @return return 3 if request header is ready and entering stream mode
 */
int HttpRequestParser::parseRequest(MsgBuffer *buf)
{
    while (true)
    {
        switch (status_)
        {
            case (HttpRequestParseStatus::kExpectMethod):
            {
                auto *space = std::find(buf->peek(),
                                        (const char *)buf->beginWrite(),
                                        ' ');
                // no space in buffer
                if (space == buf->beginWrite())
                {
                    if (buf->readableBytes() > METHOD_MAX_LEN)
                    {
                        return -k400BadRequest;
                    }
                    return 0;
                }
                // try read method
                if (!request_->setMethod(buf->peek(), space))
                {
                    return -k405MethodNotAllowed;
                }
                status_ = HttpRequestParseStatus::kExpectRequestLine;
                buf->retrieveUntil(space + 1);
                continue;
            }
            case HttpRequestParseStatus::kExpectRequestLine:
            {
                const char *crlf = buf->findCRLF();
                if (!crlf)
                {
                    if (buf->readableBytes() >= HEADER_SECTION_MAX_LEN)
                    {
                        /// The limit for request line is 64K bytes. response
                        /// k414RequestURITooLarge
                        /// TODO: Make this configurable?
                        return -k414RequestURITooLarge;
                    }
                    return 0;
                }
                if (!processRequestLine(buf->peek(), crlf))
                {
                    // error
                    return -k400BadRequest;
                }
                buf->retrieveUntil(crlf + CRLF_LEN);
                status_ = HttpRequestParseStatus::kExpectHeaders;
                continue;
            }
            case HttpRequestParseStatus::kExpectHeaders:
            {
                const char *crlf = buf->findCRLF();
                if (!crlf)
                {
                    if (buf->readableBytes() >= HEADER_LINE_MAX_LEN ||
                        buf->readableBytes() >=
                            HEADER_SECTION_MAX_LEN - headerBytes_)
                    {
                        /// Limit both an individual request header line and the
                        /// complete request header section.
                        /// TODO: Make these configurable?
                        return -k400BadRequest;
                    }
                    return 0;
                }

                const auto lineBytes =
                    static_cast<size_t>(crlf - buf->peek()) + CRLF_LEN;
                if (lineBytes > HEADER_LINE_MAX_LEN ||
                    lineBytes > HEADER_SECTION_MAX_LEN - headerBytes_)
                {
                    return -k400BadRequest;
                }
                headerBytes_ += lineBytes;

                const char *colon = std::find(buf->peek(), crlf, ':');
                // found colon
                if (colon != crlf)
                {
                    if (!isValidHttpFieldName(buf->peek(), colon))
                    {
                        // RFC 9112 Section 5.1 forbids whitespace between a
                        // field name and colon. Reject all invalid field names
                        // so they cannot be interpreted differently upstream.
                        return -k400BadRequest;
                    }
                    if (!isValidHttpFieldValue(colon + 1, crlf))
                    {
                        return -k400BadRequest;
                    }
                    const std::string_view field(buf->peek(),
                                                 static_cast<size_t>(
                                                     colon - buf->peek()));
                    if (utils::ci_equals(field, "host"))
                    {
                        if (hostHeaderSeen_)
                        {
                            return -k400BadRequest;
                        }
                        hostHeaderSeen_ = true;
                    }
                    if (utils::ci_equals(field, "content-length"))
                    {
                        if (contentLengthHeaderSeen_)
                            return -k400BadRequest;
                        contentLengthHeaderSeen_ = true;
                    }
                    if (utils::ci_equals(field, "transfer-encoding"))
                    {
                        if (transferEncodingHeaderSeen_)
                            return -k400BadRequest;
                        transferEncodingHeaderSeen_ = true;
                    }
                    request_->addHeader(buf->peek(), colon, crlf);
                    buf->retrieveUntil(crlf + CRLF_LEN);
                    continue;
                }
                if (buf->peek() != crlf)
                {
                    // Only an empty line terminates the header section.
                    return -k400BadRequest;
                }
                buf->retrieveUntil(crlf + CRLF_LEN);
                // end of headers

                // We might want a kProcessHeaders status for code readability
                // and maintainability.

                // process header information
                const auto &len = request_->getHeaderBy("content-length");
                const auto &encode = request_->getHeaderBy("transfer-encoding");
                const auto &host = request_->getHeaderBy("host");
                if ((request_->getVersion() == Version::kHttp11 &&
                     !hostHeaderSeen_) ||
                    (hostHeaderSeen_ && host.empty()))
                {
                    return -k400BadRequest;
                }
                if (transferEncodingHeaderSeen_ && encode.empty())
                {
                    return -k400BadRequest;
                }
                if (contentLengthHeaderSeen_ && transferEncodingHeaderSeen_)
                {
                    // RFC 9112 Section 6.3:
                    // Messages containing both Content-Length and
                    // Transfer-Encoding are ambiguous and might indicate
                    // request smuggling. Reject such requests to avoid
                    // inconsistent message framing.
                    return -k400BadRequest;
                }
                if (contentLengthHeaderSeen_)
                {
                    if (!utils::parseInteger(len, remainContentLength_))
                    {
                        return -k400BadRequest;
                    }
                    request_->contentLengthHeaderValue_ = remainContentLength_;
                    if (remainContentLength_ == 0 &&
                        status_ != HttpRequestParseStatus::kExpectChunkLen)
                    {
                        // content-length = 0, request is over.
                        status_ = HttpRequestParseStatus::kGotAll;
                    }
                    else
                    {
                        status_ = HttpRequestParseStatus::kExpectBody;
                    }
                }
                else
                {
                    if (!transferEncodingHeaderSeen_)
                    {
                        // no content-length and no transfer-encoding,
                        // request is over.
                        status_ = HttpRequestParseStatus::kGotAll;
                    }
                    else if (encode == "chunked")
                    {
                        status_ = HttpRequestParseStatus::kExpectChunkLen;
                    }
                    else
                    {
                        return -k501NotImplemented;
                    }
                }

                // Check max body size
                if (remainContentLength_ >
                    HttpAppFrameworkImpl::instance().getClientMaxBodySize())
                {
                    return -k413RequestEntityTooLarge;
                }

                // Check expect:100-continue
                auto &expect = request_->expect();
                if (expect == "100-continue" &&
                    request_->getVersion() >= Version::kHttp11)
                {
                    // There is nothing to negotiate when the framing indicates
                    // that the request has no body. RFC 9110 Section 10.1.1
                    // permits omitting the interim response in this case.
                    if (remainContentLength_ != 0)
                    {
                        // rfc2616-8.2.3
                        // TODO: consider adding an AOP for expect header
                        auto connPtr = conn_.lock();  // ugly
                        if (!connPtr)
                        {
                            return -1;
                        }
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k100Continue);
                        auto httpString =
                            static_cast<HttpResponseImpl *>(resp.get())
                                ->renderToBuffer();
                        connPtr->send(std::move(*httpString));
                    }
                }
                else if (!expect.empty())
                {
                    LOG_WARN << "417ExpectationFailed for \"" << expect << "\"";
                    return -k417ExpectationFailed;
                }

                assert(status_ == HttpRequestParseStatus::kGotAll ||
                       status_ == HttpRequestParseStatus::kExpectBody ||
                       status_ == HttpRequestParseStatus::kExpectChunkLen);

                if (app().isRequestStreamEnabled())
                {
                    request_->streamStart();
                    if (status_ == HttpRequestParseStatus::kGotAll)
                    {
                        ++requestsCounter_;
                        return 2;
                    }
                    else
                    {
                        return 3;
                    }
                }

                // Reserve space for full body in non-stream mode.
                // For stream mode requests that match a non-stream handler,
                // we will reserve full body before waitForStreamFinish().
                if (remainContentLength_)
                {
                    request_->reserveBodySize(remainContentLength_);
                }
                continue;
            }
            case HttpRequestParseStatus::kExpectBody:
            {
                size_t bytesToConsume =
                    remainContentLength_ <= buf->readableBytes()
                        ? remainContentLength_
                        : buf->readableBytes();
                if (bytesToConsume)
                {
                    request_->appendToBody(buf->peek(), bytesToConsume);
                    buf->retrieve(bytesToConsume);
                    remainContentLength_ -= bytesToConsume;
                }

                if (remainContentLength_ == 0)
                {
                    status_ = HttpRequestParseStatus::kGotAll;
                    ++requestsCounter_;
                    return 1;
                }
                // readableBytes() == 0, function should return.
                return 0;
            }
            case HttpRequestParseStatus::kExpectChunkLen:
            {
                const char *crlf = buf->findCRLF();
                if (!crlf)
                {
                    if (buf->readableBytes() > TRUNK_LEN_MAX_LEN + CRLF_LEN)
                    {
                        return -k400BadRequest;
                    }
                    return 0;
                }
                // chunk length line
                std::string_view len(buf->peek(), crlf - buf->peek());
                const auto extension = len.find(';');
                if (extension != std::string_view::npos)
                {
                    len = len.substr(0, extension);
                }
                if (!utils::parseInteger(len, currentChunkLength_, 16))
                {
                    return -k400BadRequest;
                }
                if (currentChunkLength_ != 0)
                {
                    const auto maxBodySize =
                        HttpAppFrameworkImpl::instance().getClientMaxBodySize();
                    if (currentChunkLength_ > maxBodySize ||
                        remainContentLength_ >
                            maxBodySize - currentChunkLength_)
                    {
                        return -k413RequestEntityTooLarge;
                    }
                    status_ = HttpRequestParseStatus::kExpectChunkBody;
                }
                else
                {
                    status_ = HttpRequestParseStatus::kExpectLastEmptyChunk;
                }
                buf->retrieveUntil(crlf + CRLF_LEN);
                continue;
            }
            case HttpRequestParseStatus::kExpectChunkBody:
            {
                const auto bytesToConsume =
                    (std::min)(currentChunkLength_, buf->readableBytes());
                if (bytesToConsume != 0)
                {
                    request_->appendToBody(buf->peek(), bytesToConsume);
                    buf->retrieve(bytesToConsume);
                    currentChunkLength_ -= bytesToConsume;
                    remainContentLength_ += bytesToConsume;
                }
                if (currentChunkLength_ != 0 || buf->readableBytes() < CRLF_LEN)
                {
                    return 0;
                }
                if (buf->peek()[0] != '\r' || buf->peek()[1] != '\n')
                {
                    // error!
                    return -k400BadRequest;
                }
                buf->retrieve(CRLF_LEN);
                status_ = HttpRequestParseStatus::kExpectChunkLen;
                continue;
            }
            case HttpRequestParseStatus::kExpectLastEmptyChunk:
            {
                const char *crlf = buf->findCRLF();
                if (!crlf)
                {
                    if (buf->readableBytes() >= HEADER_LINE_MAX_LEN ||
                        buf->readableBytes() >=
                            HEADER_SECTION_MAX_LEN - trailerBytes_)
                    {
                        return -k400BadRequest;
                    }
                    return 0;
                }
                const auto lineBytes =
                    static_cast<size_t>(crlf - buf->peek()) + CRLF_LEN;
                if (lineBytes > HEADER_LINE_MAX_LEN ||
                    lineBytes > HEADER_SECTION_MAX_LEN - trailerBytes_)
                {
                    return -k400BadRequest;
                }
                trailerBytes_ += lineBytes;
                if (crlf != buf->peek())
                {
                    const char *colon = std::find(buf->peek(), crlf, ':');
                    if (colon == buf->peek() || colon == crlf ||
                        !isValidHttpFieldName(buf->peek(), colon) ||
                        !isValidHttpFieldValue(colon + 1, crlf))
                    {
                        return -k400BadRequest;
                    }
                    // Trailers are not represented separately by
                    // HttpRequest, so validate and discard them.
                    buf->retrieveUntil(crlf + CRLF_LEN);
                    continue;
                }
                buf->retrieve(CRLF_LEN);

                if (!request_->isStreamMode())
                {
                    // Previously we only have non-stream mode, drogon handled
                    // chunked encoding internally, and give user a regular
                    // request as if it has a content-length header.
                    //
                    // We have to keep compatibility for non-stream mode.
                    //
                    // But I don't think it's a good implementation. We should
                    // instead add an api to access real content-length of
                    // requests.
                    // Now HttpRequest::realContentLength() is added, and user
                    // should no longer parse content-length header by
                    // themselves.
                    //
                    // NOTE: request forward behavior may be infected in stream
                    // mode, we should check it out.
                    request_->addHeader("content-length",
                                        std::to_string(
                                            request_->realContentLength()));
                    request_->removeHeaderBy("transfer-encoding");
                }
                status_ = HttpRequestParseStatus::kGotAll;
                ++requestsCounter_;
                return 1;
            }
            case HttpRequestParseStatus::kGotAll:
            {
                ++requestsCounter_;
                return 1;
            }
        }
    }
    return -1;  // won't reach here, just to make compiler happy
}

void HttpRequestParser::pushRequestToPipelining(const HttpRequestPtr &req,
                                                bool isHeadMethod)
{
    assert(loop_->isInLoopThread());
    requestPipelining_.push_back({req, {nullptr, isHeadMethod}});
}

/**
 * @return returns true if the the response is the first in pipeline
 */
bool HttpRequestParser::pushResponseToPipelining(const HttpRequestPtr &req,
                                                 HttpResponsePtr resp)
{
    assert(loop_->isInLoopThread());
    for (size_t i = 0; i != requestPipelining_.size(); ++i)
    {
        if (requestPipelining_[i].first == req)
        {
            requestPipelining_[i].second.first = std::move(resp);
            return i == 0;
        }
    }
    assert(false);  // Should always find a match
    return false;
}

void HttpRequestParser::popReadyResponses(
    std::vector<std::pair<HttpResponsePtr, bool>> &buffer)
{
    while (!requestPipelining_.empty() &&
           requestPipelining_.front().second.first)
    {
        buffer.push_back(std::move(requestPipelining_.front().second));
        requestPipelining_.pop_front();
    }
}
