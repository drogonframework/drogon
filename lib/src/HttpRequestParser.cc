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

using namespace trantor;
using namespace drogon;

static constexpr size_t CRLF_LEN = 2;            // strlen("crlf")
static constexpr size_t METHOD_MAX_LEN = 7;      // strlen("OPTIONS")
static constexpr size_t TRUNK_LEN_MAX_LEN = 16;  // 0xFFFFFFFF,FFFFFFFF

HttpRequestParser::HttpRequestParser(const trantor::TcpConnectionPtr &connPtr)
    : status_(HttpRequestParseStatus::kExpectMethod),
      loop_(connPtr->getLoop()),
      conn_(connPtr)
{
}

void HttpRequestParser::shutdownConnection(HttpStatusCode code)
{
    auto connPtr = conn_.lock();
    if (connPtr)
    {
        connPtr->send(utils::formattedString(
            "HTTP/1.1 %d %s\r\nConnection: close\r\n\r\n",
            code,
            statusCodeToString(code).data()));
        connPtr->shutdown();
    }
}

bool HttpRequestParser::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end)
    {
        const char *slash = std::find(start, space, '/');
        if (slash != start && slash + 1 < space && *(slash + 1) == '/')
        {
            // scheme precedents
            slash = std::find(slash + 2, space, '/');
        }
        const char *question = std::find(slash, space, '?');
        if (slash != space)
        {
            request_->setPath(slash, question);
        }
        else
        {
            // An empty abs_path is equivalent to an abs_path of "/"
            request_->setPath("/");
        }
        if (question != space)
        {
            request_->setQuery(question + 1, space);
        }
        start = space + 1;
        succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
        if (succeed)
        {
            if (*(end - 1) == '1')
            {
                request_->setVersion(Version::kHttp11);
            }
            else if (*(end - 1) == '0')
            {
                request_->setVersion(Version::kHttp10);
            }
            else
            {
                succeed = false;
            }
        }
    }
    return succeed;
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
    currentContentLength_ = 0;
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
 * @return return -1 if encounters any error in request
 * @return return 0 if request is not ready
 * @return return 1 if request is ready
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
                        buf->retrieveAll();
                        shutdownConnection(k400BadRequest);
                        return -1;
                    }
                    return 0;
                }
                // try read method
                if (!request_->setMethod(buf->peek(), space))
                {
                    buf->retrieveAll();
                    shutdownConnection(k405MethodNotAllowed);
                    return -1;
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
                    if (buf->readableBytes() >= 64 * 1024)
                    {
                        /// The limit for request line is 64K bytes. response
                        /// k414RequestURITooLarge
                        /// TODO: Make this configurable?
                        buf->retrieveAll();
                        shutdownConnection(k414RequestURITooLarge);
                        return -1;
                    }
                    return 0;
                }
                if (!processRequestLine(buf->peek(), crlf))
                {
                    // error
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return -1;
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
                    if (buf->readableBytes() >= 64 * 1024)
                    {
                        /// The limit for every request header is 64K bytes;
                        /// TODO: Make this configurable?
                        buf->retrieveAll();
                        shutdownConnection(k400BadRequest);
                        return -1;
                    }
                    return 0;
                }

                const char *colon = std::find(buf->peek(), crlf, ':');
                // found colon
                if (colon != crlf)
                {
                    request_->addHeader(buf->peek(), colon, crlf);
                    buf->retrieveUntil(crlf + CRLF_LEN);
                    continue;
                }
                buf->retrieveUntil(crlf + CRLF_LEN);
                // end of headers

                // We might want a kProcessHeaders status for code readability
                // and maintainability.

                // process header information
                auto &len = request_->getHeaderBy("content-length");
                if (!len.empty())
                {
                    try
                    {
                        currentContentLength_ =
                            static_cast<size_t>(std::stoull(len));
                    }
                    catch (...)
                    {
                        buf->retrieveAll();
                        shutdownConnection(k400BadRequest);
                        return -1;
                    }
                    if (currentContentLength_ == 0)
                    {
                        // content-length = 0, request is over.
                        status_ = HttpRequestParseStatus::kGotAll;
                        ++requestsCounter_;
                        return 1;
                    }
                    else
                    {
                        status_ = HttpRequestParseStatus::kExpectBody;
                    }
                }
                else
                {
                    const std::string &encode =
                        request_->getHeaderBy("transfer-encoding");
                    if (encode.empty())
                    {
                        // no content-length and no transfer-encoding,
                        // request is over.
                        status_ = HttpRequestParseStatus::kGotAll;
                        ++requestsCounter_;
                        return 1;
                    }
                    else if (encode == "chunked")
                    {
                        status_ = HttpRequestParseStatus::kExpectChunkLen;
                    }
                    else
                    {
                        buf->retrieveAll();
                        shutdownConnection(k501NotImplemented);
                        return -1;
                    }
                }

                auto &expect = request_->expect();
                if (expect == "100-continue" &&
                    request_->getVersion() >= Version::kHttp11)
                {
                    if (currentContentLength_ == 0)
                    {
                        // error
                        buf->retrieveAll();
                        shutdownConnection(k400BadRequest);
                        return -1;
                    }
                    // rfc2616-8.2.3
                    auto connPtr = conn_.lock();
                    if (!connPtr)
                    {
                        return -1;
                    }
                    auto resp = HttpResponse::newHttpResponse();
                    if (currentContentLength_ >
                        HttpAppFrameworkImpl::instance().getClientMaxBodySize())
                    {
                        resp->setStatusCode(k413RequestEntityTooLarge);
                        auto httpString =
                            static_cast<HttpResponseImpl *>(resp.get())
                                ->renderToBuffer();
                        reset();
                        connPtr->send(std::move(*httpString));
                        // TODO: missing logic here
                    }
                    else
                    {
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
                    buf->retrieveAll();
                    shutdownConnection(k417ExpectationFailed);
                    return -1;
                }
                else if (currentContentLength_ >
                         HttpAppFrameworkImpl::instance()
                             .getClientMaxBodySize())
                {
                    buf->retrieveAll();
                    shutdownConnection(k413RequestEntityTooLarge);
                    return -1;
                }
                request_->reserveBodySize(currentContentLength_);
                continue;
            }
            case HttpRequestParseStatus::kExpectBody:
            {
                size_t bytesToConsume =
                    currentContentLength_ <= buf->readableBytes()
                        ? currentContentLength_
                        : buf->readableBytes();
                if (bytesToConsume)
                {
                    request_->appendToBody(buf->peek(), bytesToConsume);
                    buf->retrieve(bytesToConsume);
                    currentContentLength_ -= bytesToConsume;
                }

                if (currentContentLength_ == 0)
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
                        buf->retrieveAll();
                        shutdownConnection(k400BadRequest);
                        return -1;
                    }
                    return 0;
                }
                // chunk length line
                std::string len(buf->peek(), crlf - buf->peek());
                char *end;
                currentChunkLength_ = strtol(len.c_str(), &end, 16);
                if (currentChunkLength_ != 0)
                {
                    if (currentChunkLength_ + currentContentLength_ >
                        HttpAppFrameworkImpl::instance().getClientMaxBodySize())
                    {
                        buf->retrieveAll();
                        shutdownConnection(k413RequestEntityTooLarge);
                        return -1;
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
                if (buf->readableBytes() < (currentChunkLength_ + CRLF_LEN))
                {
                    return 0;
                }
                if (*(buf->peek() + currentChunkLength_) != '\r' ||
                    *(buf->peek() + currentChunkLength_ + 1) != '\n')
                {
                    // error!
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return -1;
                }
                request_->appendToBody(buf->peek(), currentChunkLength_);
                buf->retrieve(currentChunkLength_ + CRLF_LEN);
                currentContentLength_ += currentChunkLength_;
                currentChunkLength_ = 0;
                status_ = HttpRequestParseStatus::kExpectChunkLen;
                continue;
            }
            case HttpRequestParseStatus::kExpectLastEmptyChunk:
            {
                // last empty chunk
                if (buf->readableBytes() < CRLF_LEN)
                {
                    return 0;
                }
                if (*(buf->peek()) != '\r' || *(buf->peek() + 1) != '\n')
                {
                    // error!
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return -1;
                }
                buf->retrieve(CRLF_LEN);
                status_ = HttpRequestParseStatus::kGotAll;
                request_->addHeader("content-length",
                                    std::to_string(request_->bodyLength()));
                request_->removeHeaderBy("transfer-encoding");
                ++requestsCounter_;
                return 1;
            }
            case HttpRequestParseStatus::kGotAll:
            {
                return 1;
            }
        }
    }
    return -1;
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
