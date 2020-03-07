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
#include "HttpAppFrameworkImpl.h"
#include "HttpResponseImpl.h"
#include "HttpRequestImpl.h"
#include "HttpUtils.h"
#include <drogon/HttpTypes.h>
#include <iostream>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>

using namespace trantor;
using namespace drogon;

HttpRequestParser::HttpRequestParser(const trantor::TcpConnectionPtr &connPtr)
    : status_(HttpRequestParseStatus::ExpectMethod),
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
        const char *question = std::find(start, space, '?');
        if (question != space)
        {
            request_->setPath(start, question);
            request_->setQuery(question + 1, space);
        }
        else
        {
            request_->setPath(start, space);
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
    std::weak_ptr<HttpRequestParser> weakPtr = shared_from_this();
    return std::shared_ptr<HttpRequestImpl>(ptr, [weakPtr](HttpRequestImpl *p) {
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
                thisPtr->loop_->queueInLoop([thisPtr, p]() {
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
    status_ = HttpRequestParseStatus::ExpectMethod;
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
// Return false if any error
bool HttpRequestParser::parseRequest(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    //  std::cout<<std::string(buf->peek(),buf->readableBytes())<<std::endl;
    while (hasMore)
    {
        if (status_ == HttpRequestParseStatus::ExpectMethod)
        {
            auto *space =
                std::find(buf->peek(), (const char *)buf->beginWrite(), ' ');
            if (space != buf->beginWrite())
            {
                if (request_->setMethod(buf->peek(), space))
                {
                    status_ = HttpRequestParseStatus::ExpectRequestLine;
                    buf->retrieveUntil(space + 1);
                    continue;
                }
                else
                {
                    buf->retrieveAll();
                    shutdownConnection(k405MethodNotAllowed);
                    return false;
                }
            }
            else
            {
                if (buf->readableBytes() >= 7)
                {
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return false;
                }
                hasMore = false;
            }
        }
        else if (status_ == HttpRequestParseStatus::ExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    buf->retrieveUntil(crlf + 2);
                    status_ = HttpRequestParseStatus::ExpectHeaders;
                }
                else
                {
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return false;
                }
            }
            else
            {
                if (buf->readableBytes() >= 64 * 1024)
                {
                    /// The limit for request line is 64K bytes. respone
                    /// k414RequestURITooLarge
                    /// TODO: Make this configurable?
                    buf->retrieveAll();
                    shutdownConnection(k414RequestURITooLarge);
                    return false;
                }
                hasMore = false;
            }
        }
        else if (status_ == HttpRequestParseStatus::ExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_->addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header

                    if (request_->contentLen_ == 0)
                    {
                        status_ = HttpRequestParseStatus::GotAll;
                        ++requestsCounter_;
                        hasMore = false;
                    }
                    else
                    {
                        status_ = HttpRequestParseStatus::ExpectBody;
                    }

                    auto &expect = request_->expect();
                    if (expect == "100-continue" &&
                        request_->getVersion() >= Version::kHttp11)
                    {
                        if (request_->contentLen_ == 0)
                        {
                            buf->retrieveAll();
                            shutdownConnection(k400BadRequest);
                            return false;
                        }
                        // rfc2616-8.2.3
                        auto connPtr = conn_.lock();
                        if (connPtr)
                        {
                            auto resp = HttpResponse::newHttpResponse();
                            if (request_->contentLen_ >
                                HttpAppFrameworkImpl::instance()
                                    .getClientMaxBodySize())
                            {
                                resp->setStatusCode(k413RequestEntityTooLarge);
                                auto httpString =
                                    static_cast<HttpResponseImpl *>(resp.get())
                                        ->renderToBuffer();
                                reset();
                                connPtr->send(std::move(*httpString));
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
                    }
                    else if (!expect.empty())
                    {
                        LOG_WARN << "417ExpectationFailed for \"" << expect
                                 << "\"";
                        auto connPtr = conn_.lock();
                        if (connPtr)
                        {
                            buf->retrieveAll();
                            shutdownConnection(k417ExpectationFailed);
                            return false;
                        }
                    }
                    else if (request_->contentLen_ >
                             HttpAppFrameworkImpl::instance()
                                 .getClientMaxBodySize())
                    {
                        buf->retrieveAll();
                        shutdownConnection(k413RequestEntityTooLarge);
                        return false;
                    }
                    request_->reserveBodySize();
                }

                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                if (buf->readableBytes() >= 64 * 1024)
                {
                    /// The limit for every request header is 64K bytes;
                    /// TODO: Make this configurable?
                    buf->retrieveAll();
                    shutdownConnection(k400BadRequest);
                    return false;
                }
                hasMore = false;
            }
        }
        else if (status_ == HttpRequestParseStatus::ExpectBody)
        {
            if (buf->readableBytes() == 0)
            {
                if (request_->contentLen_ == 0)
                {
                    status_ = HttpRequestParseStatus::GotAll;
                    ++requestsCounter_;
                }
                break;
            }
            if (request_->contentLen_ >= buf->readableBytes())
            {
                request_->contentLen_ -= buf->readableBytes();
                request_->appendToBody(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                request_->appendToBody(buf->peek(), request_->contentLen_);
                buf->retrieve(request_->contentLen_);
                request_->contentLen_ = 0;
            }
            if (request_->contentLen_ == 0)
            {
                status_ = HttpRequestParseStatus::GotAll;
                ++requestsCounter_;
                hasMore = false;
            }
        }
    }
    return ok;
}

void HttpRequestParser::pushRquestToPipelining(const HttpRequestPtr &req)
{
#ifndef NDEBUG
    auto conn = conn_.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    requestPipelining_.push_back({req, {nullptr, false}});
}

HttpRequestPtr HttpRequestParser::getFirstRequest() const
{
#ifndef NDEBUG
    auto conn = conn_.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!requestPipelining_.empty())
    {
        return requestPipelining_.front().first;
    }
    return nullptr;
}

std::pair<HttpResponsePtr, bool> HttpRequestParser::getFirstResponse() const
{
#ifndef NDEBUG
    auto conn = conn_.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!requestPipelining_.empty())
    {
        return requestPipelining_.front().second;
    }
    return {nullptr, false};
}

void HttpRequestParser::popFirstRequest()
{
#ifndef NDEBUG
    auto conn = conn_.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    requestPipelining_.pop_front();
}

void HttpRequestParser::pushResponseToPipelining(const HttpRequestPtr &req,
                                                 const HttpResponsePtr &resp,
                                                 bool isHeadMethod)
{
#ifndef NDEBUG
    auto conn = conn_.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    for (auto &iter : requestPipelining_)
    {
        if (iter.first == req)
        {
            iter.second = {resp, isHeadMethod};
            return;
        }
    }
}