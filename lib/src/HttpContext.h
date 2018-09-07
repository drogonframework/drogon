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
#include <drogon/WebSocketConnection.h>
#include <list>
#include <mutex>
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

        HttpContext();


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
          res_state_ = HttpResponseParseState::kExpectResponseLine;
          request_.reset(new HttpRequestImpl);
          HttpResponseImpl dummy_res;
          response_.swap(dummy_res);
        }

        void resetRes()
        {
          res_state_ = HttpResponseParseState::kExpectResponseLine;
          response_.clear();
        }

        const HttpRequestPtr request() const
        {
          return request_;
        }

        HttpRequestPtr request()
        {
          return request_;
        }

        HttpRequestImplPtr requestImpl()
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
        bool firstReq()
        {
            if(_firstRequest)
            {
                _firstRequest=false;
                return true;
            }
            return false;
        }
        WebSocketConnectionPtr webSocketConn()
        {
            return _websockConnPtr;
        }
        void setWebsockConnection(const WebSocketConnectionPtr &conn)
        {
            _websockConnPtr=conn;
        }
        //to support request pipelining(rfc2616-8.1.2.2)
        std::mutex & getPipeLineMutex();
        void pushRquestToPipeLine(const HttpRequestPtr &req);
        HttpRequestPtr getFirstRequest() const;
        HttpResponsePtr getFirstResponse() const;
        void popFirstRequest();
        void pushResponseToPipeLine(const HttpRequestPtr &req,const HttpResponsePtr &resp);
        private:
        bool processRequestLine(const char *begin, const char *end);
        bool processResponseLine(const char *begin, const char *end);

        HttpRequestParseState state_;
        HttpRequestImplPtr request_;

        HttpResponseParseState res_state_;
        HttpResponseImpl response_;
        bool _firstRequest=true;
        WebSocketConnectionPtr _websockConnPtr;

        std::list<std::pair<HttpRequestPtr,HttpResponsePtr>> _requestPipeLine;
        std::shared_ptr<std::mutex> _pipeLineMutex;
    };

}

#endif // MUDUO_NET_HTTP_HTTPCONTEXT_H
