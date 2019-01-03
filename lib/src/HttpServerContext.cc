/**
 *
 *  HttpServerContext.cc
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

#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/Logger.h>
#include "HttpServerContext.h"
#include "HttpResponseImpl.h"
#include <iostream>
using namespace trantor;
using namespace drogon;
HttpServerContext::HttpServerContext(const trantor::TcpConnectionPtr &connPtr)
    : _state(kExpectRequestLine),
      _request(new HttpRequestImpl),
      _conn(connPtr)
{
}
bool HttpServerContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && _request->setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
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
    }
    return succeed;
}

// return false if any error
bool HttpServerContext::parseRequest(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    //  std::cout<<std::string(buf->peek(),buf->readableBytes())<<std::endl;
    while (hasMore)
    {
        if (_state == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    //_request->setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    _state = kExpectHeaders;
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
        else if (_state == kExpectHeaders)
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
                        _state = kExpectBody;
                        auto &expect = _request->getHeaderBy("expect");
                        if (expect == "100-continue" &&
                            _request->getVersion() >= HttpRequest::kHttp11)
                        {
                            //rfc2616-8.2.3
                            //TODO:here we can add content-length limitation
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                auto resp = HttpResponse::newHttpResponse();
                                resp->setStatusCode(HttpResponse::k100Continue);
                                auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                connPtr->send(httpString);
                            }
                        }
                        else if (!expect.empty())
                        {
                            LOG_WARN << "417ExpectationFailed for \"" << expect << "\"";
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                auto resp = HttpResponse::newHttpResponse();
                                resp->setStatusCode(HttpResponse::k417ExpectationFailed);
                                MsgBuffer buffer;
                                auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                connPtr->send(httpString);
                                buf->retrieveAll();
                                connPtr->forceClose();

                                //return false;
                            }
                        }
                    }
                    else
                    {
                        _state = kGotAll;
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
        else if (_state == kExpectBody)
        {
            if (buf->readableBytes() == 0)
            {
                if (_request->_contentLen == 0)
                {
                    _state = kGotAll;
                }
                break;
            }
            if (_request->_contentLen >= buf->readableBytes())
            {
                _request->_contentLen -= buf->readableBytes();
                _request->_content += std::string(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                _request->_content += std::string(buf->peek(), _request->_contentLen);
                buf->retrieve(_request->_contentLen);
                _request->_contentLen = 0;
            }
            if (_request->_contentLen == 0)
            {
                _state = kGotAll;
                hasMore = false;
            }
        }
    }

    return ok;
}

void HttpServerContext::pushRquestToPipeLine(const HttpRequestPtr &req)
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    std::pair<HttpRequestPtr, HttpResponsePtr> reqPair(req, HttpResponseImplPtr());

    _requestPipeLine.push_back(std::move(reqPair));
}
HttpRequestPtr HttpServerContext::getFirstRequest() const
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!_requestPipeLine.empty())
    {
        return _requestPipeLine.front().first;
    }
    return HttpRequestImplPtr();
}
HttpResponsePtr HttpServerContext::getFirstResponse() const
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    if (!_requestPipeLine.empty())
    {
        return _requestPipeLine.front().second;
    }
    return HttpResponseImplPtr();
}
void HttpServerContext::popFirstRequest()
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    _requestPipeLine.pop_front();
}
void HttpServerContext::pushResponseToPipeLine(const HttpRequestPtr &req,
                                               const HttpResponsePtr &resp)
{
#ifndef NDEBUG
    auto conn = _conn.lock();
    if (conn)
    {
        conn->getLoop()->assertInLoopThread();
    }
#endif
    for (auto &iter : _requestPipeLine)
    {
        if (iter.first == req)
        {
            iter.second = resp;
            return;
        }
    }
}

// std::mutex &HttpServerContext::getPipeLineMutex()
// {
//     return *_pipeLineMutex;
// }
