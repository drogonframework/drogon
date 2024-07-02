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
    remainContentLength_ = 0;
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
                    if (buf->readableBytes() >= 64 * 1024)
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
                    if (buf->readableBytes() >= 64 * 1024)
                    {
                        /// The limit for every request header is 64K bytes;
                        /// TODO: Make this configurable?
                        return -k400BadRequest;
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
                        remainContentLength_ =
                            static_cast<size_t>(std::stoull(len));
                    }
                    catch (...)
                    {
                        return -k400BadRequest;
                    }
                    request_->contentLengthHeaderValue_ = remainContentLength_;
                    if (remainContentLength_ == 0)
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
                    const std::string &encode =
                        request_->getHeaderBy("transfer-encoding");
                    if (encode.empty())
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
                    if (remainContentLength_ == 0)
                    {
                        // error
                        return -k400BadRequest;
                    }
                    else
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
                std::string len(buf->peek(), crlf - buf->peek());
                char *end;
                currentChunkLength_ = strtol(len.c_str(), &end, 16);
                if (currentChunkLength_ != 0)
                {
                    if (currentChunkLength_ + remainContentLength_ >
                        HttpAppFrameworkImpl::instance().getClientMaxBodySize())
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
                if (buf->readableBytes() < (currentChunkLength_ + CRLF_LEN))
                {
                    return 0;
                }
                if (*(buf->peek() + currentChunkLength_) != '\r' ||
                    *(buf->peek() + currentChunkLength_ + 1) != '\n')
                {
                    // error!
                    return -k400BadRequest;
                }
                request_->appendToBody(buf->peek(), currentChunkLength_);
                buf->retrieve(currentChunkLength_ + CRLF_LEN);
                remainContentLength_ += currentChunkLength_;
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
                    return -k400BadRequest;
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
