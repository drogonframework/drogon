// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

//taken from muduo and modified

/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
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
class HttpClientContext
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

    HttpClientContext(const trantor::TcpConnectionPtr &connPtr);

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


    const HttpResponsePtr response() const
    {
        return _response;
    }

    HttpResponsePtr response()
    {
        return _response;
    }
    HttpResponseImplPtr responseImpl()
    {
        return _response;
    }
    
  private:
    bool processResponseLine(const char *begin, const char *end);

    HttpResponseParseState _state;
    HttpResponseImplPtr _response;

    std::weak_ptr<trantor::TcpConnection> _conn;
};

} // namespace drogon
