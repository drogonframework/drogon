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

#ifndef MUDUO_NET_HTTP_HTTPCONTEXT_H
#define MUDUO_NET_HTTP_HTTPCONTEXT_H
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <trantor/utils/MsgBuffer.h>

using namespace trantor;
namespace drogon
{
    class HttpContext
    {
    public:
        enum HttpRequestParseState
        {
            kExpectRequestLine,
            kExpectHeaders,
            kExpectBody,
            kGotAll,
        };

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

        HttpContext()
                : state_(kExpectRequestLine),
                  res_state_(HttpResponseParseState::kExpectResponseLine)
        {
        }

        // default copy-ctor, dtor and assignment are fine

        // return false if any error
        bool parseRequest(MsgBuffer *buf);
        bool parseResponse(MsgBuffer *buf);

        bool gotAll() const
        {
          return state_ == kGotAll;
        }

        bool resGotAll() const
        {
          return res_state_ == HttpResponseParseState::kGotAll;
        }

        bool resExpectResponseLine() const
        {
          return res_state_ == HttpResponseParseState::kExpectResponseLine;
        }

        void reset()
        {
          state_ = kExpectRequestLine;
          HttpRequestImpl dummy;
          request_.swap(dummy);
        }

        void resetRes()
        {
          res_state_ = HttpResponseParseState::kExpectResponseLine;
          response_.clear();
        }

        const HttpRequest &request() const
        {
          return request_;
        }

        HttpRequest &request()
        {
          return request_;
        }

        HttpRequestImpl &requestImpl()
        {
            return request_;
        }

        const HttpResponse &response() const
        {
          return response_;
        }

        HttpResponse &response()
        {
          return response_;
        }
        HttpResponseImpl &responseImpl()
        {
            return response_;
        }

    private:
        bool processRequestLine(const char *begin, const char *end);
        bool processResponseLine(const char *begin, const char *end);

        HttpRequestParseState state_;
        HttpRequestImpl request_;

        HttpResponseParseState res_state_;
        HttpResponseImpl response_;
    };

}

#endif // MUDUO_NET_HTTP_HTTPCONTEXT_H
