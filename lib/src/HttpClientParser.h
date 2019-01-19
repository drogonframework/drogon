/**
 *
 *  HttpClientParser.h
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

#pragma once

#include "HttpResponseImpl.h"
#include <trantor/utils/MsgBuffer.h>
#include <drogon/WebSocketConnection.h>
#include <list>
#include <mutex>
#include <trantor/net/TcpConnection.h>

using namespace trantor;
namespace drogon
{
class HttpClientParser
{
  public:
    enum class HttpResponseParseState
    {
        kExpectResponseLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkLen,
        kExpectChunkBody,
        kExpectLastEmptyChunk,
        kExpectClose,
        kGotAll,
    };

    explicit HttpClientParser(const trantor::TcpConnectionPtr &connPtr);

    // default copy-ctor, dtor and assignment are fine

    // return false if any error
    bool parseResponse(MsgBuffer *buf);

    bool gotAll() const
    {
        return _state == HttpResponseParseState::kGotAll;
    }

    void reset()
    {
        _state = HttpResponseParseState::kExpectResponseLine;
        _response.reset(new HttpResponseImpl);
    }

    const HttpResponseImplPtr &responseImpl() const
    {
        return _response;
    }

  private:
    bool processResponseLine(const char *begin, const char *end);

    HttpResponseParseState _state;
    HttpResponseImplPtr _response;
};

} // namespace drogon
