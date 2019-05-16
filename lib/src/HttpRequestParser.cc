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

#include <drogon/HttpTypes.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/Logger.h>
#include "HttpAppFrameworkImpl.h"
#include "HttpRequestParser.h"
#include "HttpResponseImpl.h"
#include "HttpUtils.h"
#include <iostream>

using namespace trantor;
using namespace drogon;

HttpRequestParser::HttpRequestParser(const trantor::TcpConnectionPtr &connPtr)
    : _state(HttpRequestParseState_ExpectMethod),
      _loop(connPtr->getLoop()),
      _request(new HttpRequestImpl(connPtr->getLoop())),
      _conn(connPtr)
{
}

void HttpRequestParser::shutdownConnection(HttpStatusCode code)
{
    auto connPtr = _conn.lock();
    if (connPtr)
    {
        connPtr->send(utils::formattedString("HTTP/1.1 %d %s\r\n\r\n", code, statusCodeToString(code).data()));
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
            _request->setPath(start, question);
            _request->setQuery(question + 1, space);
        }
        else
        {
            _request->setPath(start, space);
        }
        start = space + 1;
        succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
        if (succeed)
        {
            if (*(end - 1) == '1')
            {
                _request->setVersion(HttpRequest::kHttp11);
            }
            else if (*(end - 1) == '0')
            {
                _request->setVersion(HttpRequest::kHttp10);
            }
            else
            {
                succeed = false;
            }
        }
    }
    return succeed;
}

// Return false if any error
bool HttpRequestParser::parseRequest(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    //  std::cout<<std::string(buf->peek(),buf->readableBytes())<<std::endl;
    while (hasMore)
    {
        if (_state == HttpRequestParseState_ExpectMethod)
        {
            auto *space = std::find(buf->peek(), (const char *)buf->beginWrite(), ' ');
            if (space != buf->beginWrite())
            {
                if (_request->setMethod(buf->peek(), space))
                {
                    _state = HttpRequestParseState_ExpectRequestLine;
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
        else if (_state == HttpRequestParseState_ExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    buf->retrieveUntil(crlf + 2);
                    _state = HttpRequestParseState_ExpectHeaders;
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
                    /// The limit for request line is 64K bytes. respone k414RequestURITooLarge
                    /// TODO: Make this configurable?
                    buf->retrieveAll();
                    shutdownConnection(k414RequestURITooLarge);
                    return false;
                }
                hasMore = false;
            }
        }
        else if (_state == HttpRequestParseState_ExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    _request->addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    const std::string &len = _request->getHeaderBy("content-length");
                    LOG_TRACE << "content len=" << len;
                    if (!len.empty())
                    {
                        _request->_contentLen = atoi(len.c_str());
                        _state = HttpRequestParseState_ExpectBody;
                        auto &expect = _request->getHeaderBy("expect");
                        if (expect == "100-continue" &&
                            _request->getVersion() >= HttpRequest::kHttp11)
                        {
                            if (_request->_contentLen == 0)
                            {
                                buf->retrieveAll();
                                shutdownConnection(k400BadRequest);
                                return false;
                            }
                            //rfc2616-8.2.3
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                auto resp = HttpResponse::newHttpResponse();
                                if (_request->_contentLen > HttpAppFrameworkImpl::instance().getClientMaxBodySize())
                                {
                                    resp->setStatusCode(k413RequestEntityTooLarge);
                                    auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                    reset();
                                    connPtr->send(httpString);
                                }
                                else
                                {
                                    resp->setStatusCode(k100Continue);
                                    auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                    connPtr->send(httpString);
                                }
                            }
                        }
                        else if (!expect.empty())
                        {
                            LOG_WARN << "417ExpectationFailed for \"" << expect << "\"";
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                buf->retrieveAll();
                                shutdownConnection(k417ExpectationFailed);
                                return false;
                            }
                        }
                        else if (_request->_contentLen > HttpAppFrameworkImpl::instance().getClientMaxBodySize())
                        {
                            buf->retrieveAll();
                            shutdownConnection(k413RequestEntityTooLarge);
                            return false;
                        }
                        _request->_content.reserve(_request->_contentLen);
                    }
                    else
                    {
                        _state = HttpRequestParseState_GotAll;
                        _requestsCounter++;
                        hasMore = false;
                    }
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
        else if (_state == HttpRequestParseState_ExpectBody)
        {
            if (buf->readableBytes() == 0)
            {
                if (_request->_contentLen == 0)
                {
                    _state = HttpRequestParseState_GotAll;
                    _requestsCounter++;
                }
                break;
            }
            if (_request->_contentLen >= buf->readableBytes())
            {
                _request->_contentLen -= buf->readableBytes();
                _request->_content.append(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                _request->_content.append(buf->peek(), _request->_contentLen);
                buf->retrieve(_request->_contentLen);
                _request->_contentLen = 0;
            }
            if (_request->_contentLen == 0)
            {
                _state = HttpRequestParseState_GotAll;
                _requestsCounter++;
                hasMore = false;
            }
        }
    }
    return ok;
}

void HttpRequestParser::pushRquestToPipelining(const HttpRequestPtr &req)
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    std::pair<HttpRequestPtr, HttpResponsePtr> reqPair(req, HttpResponseImplPtr());

    _requestPipelining.push_back(std::move(reqPair));
}

HttpRequestPtr HttpRequestParser::getFirstRequest() const
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!_requestPipelining.empty())
    {
        return _requestPipelining.front().first;
    }
    return HttpRequestImplPtr();
}

HttpResponsePtr HttpRequestParser::getFirstResponse() const
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!_requestPipelining.empty())
    {
        return _requestPipelining.front().second;
    }
    return HttpResponseImplPtr();
}

void HttpRequestParser::popFirstRequest()
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    _requestPipelining.pop_front();
}

void HttpRequestParser::pushResponseToPipelining(const HttpRequestPtr &req,
                                                 const HttpResponsePtr &resp)
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    for (auto &iter : _requestPipelining)
    {
        if (iter.first == req)
        {
            iter.second = resp;
            return;
        }
    }
}