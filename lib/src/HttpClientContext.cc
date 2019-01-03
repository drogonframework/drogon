/**
 *
 *  HttpClientContext.cc
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
#include "HttpClientContext.h"
#include <iostream>
using namespace trantor;
using namespace drogon;
HttpClientContext::HttpClientContext(const trantor::TcpConnectionPtr &connPtr)
    : _state(HttpResponseParseState::kExpectResponseLine),
      _response(new HttpResponseImpl)
{
}

bool HttpClientContext::processResponseLine(const char *begin, const char *end)
{
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end)
    {
        LOG_TRACE << *(space - 1);
        if (*(space - 1) == '1')
        {
            _response->setVersion(HttpResponse::kHttp11);
        }
        else if (*(space - 1) == '0')
        {
            _response->setVersion(HttpResponse::kHttp10);
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
        _response->setStatusCode(HttpResponse::HttpStatusCode(code), status_message);

        return true;
    }
    return false;
}

// return false if any error
bool HttpClientContext::parseResponse(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        if (_state == HttpResponseParseState::kExpectResponseLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processResponseLine(buf->peek(), crlf);
                if (ok)
                {
                    //_response->setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    _state = HttpResponseParseState::kExpectHeaders;
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
        else if (_state == HttpResponseParseState::kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    _response->addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    const std::string &len = _response->getHeaderBy("content-length");
                    //LOG_INFO << "content len=" << len;
                    if (!len.empty())
                    {
                        _response->_left_body_length = atoi(len.c_str());
                        _state = HttpResponseParseState::kExpectBody;
                    }
                    else
                    {
                        const std::string &encode = _response->getHeaderBy("transfer-encoding");
                        if (encode == "chunked")
                        {
                            _state = HttpResponseParseState::kExpectChunkLen;
                            hasMore = true;
                        }
                        else
                        {
                            _state = HttpResponseParseState::kExpectClose;
                            hasMore = true;
                        }
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (_state == HttpResponseParseState::kExpectBody)
        {
            //LOG_INFO << "expectBody:len=" << request_->contentLen;
            //LOG_INFO << "expectBody:buf=" << buf;
            if (buf->readableBytes() == 0)
            {
                if (_response->_left_body_length == 0)
                {
                    _state = HttpResponseParseState::kGotAll;
                }
                break;
            }
            if (_response->_left_body_length >= buf->readableBytes())
            {
                _response->_left_body_length -= buf->readableBytes();
                _response->_bodyPtr->append(std::string(buf->peek(), buf->readableBytes()));
                buf->retrieveAll();
            }
            else
            {
                _response->_bodyPtr->append(std::string(buf->peek(), _response->_left_body_length));
                buf->retrieve(_response->_left_body_length);
                _response->_left_body_length = 0;
            }
            if (_response->_left_body_length == 0)
            {
                _state = HttpResponseParseState::kGotAll;
                LOG_TRACE << "post got all:len=" << _response->_left_body_length;
                //LOG_INFO<<"content:"<<request_->content_;
                LOG_TRACE << "content(END)";
                hasMore = false;
            }
        }
        else if (_state == HttpResponseParseState::kExpectClose)
        {
            _response->_bodyPtr->append(std::string(buf->peek(), buf->readableBytes()));
            buf->retrieveAll();
            break;
        }
        else if (_state == HttpResponseParseState::kExpectChunkLen)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                //chunk length line
                std::string len(buf->peek(), crlf - buf->peek());
                char *end;
                _response->_current_chunk_length = strtol(len.c_str(), &end, 16);
                //LOG_TRACE << "chun length : " << _response->_current_chunk_length;
                if (_response->_current_chunk_length != 0)
                {
                    _state = HttpResponseParseState::kExpectChunkBody;
                }
                else
                {
                    _state = HttpResponseParseState::kExpectLastEmptyChunk;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (_state == HttpResponseParseState::kExpectChunkBody)
        {
            //LOG_TRACE<<"expect chunk len="<<_response->_current_chunk_length;
            if (buf->readableBytes() >= (_response->_current_chunk_length + 2))
            {
                if (*(buf->peek() + _response->_current_chunk_length) == '\r' &&
                    *(buf->peek() + _response->_current_chunk_length + 1) == '\n')
                {
                    _response->_bodyPtr->append(std::string(buf->peek(), _response->_current_chunk_length));
                    buf->retrieve(_response->_current_chunk_length + 2);
                    _response->_current_chunk_length = 0;
                    _state = HttpResponseParseState::kExpectChunkLen;
                }
                else
                {
                    //error!
                    buf->retrieveAll();
                    return false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (_state == HttpResponseParseState::kExpectLastEmptyChunk)
        {
            //last empty chunk
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                buf->retrieveUntil(crlf + 2);
                _state = HttpResponseParseState::kGotAll;
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
