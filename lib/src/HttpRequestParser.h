/**
 *
 *  HttpRequestParser.h
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

#include "HttpRequestImpl.h"
#include <trantor/utils/MsgBuffer.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/HttpResponse.h>
#include <deque>
#include <mutex>
#include <trantor/net/TcpConnection.h>

using namespace trantor;
namespace drogon
{
class HttpRequestParser
{
  public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    explicit HttpRequestParser(const trantor::TcpConnectionPtr &connPtr);

    // return false if any error
    bool parseRequest(MsgBuffer *buf);

    bool gotAll() const
    {
        return _state == kGotAll;
    }

    void reset()
    {
        _state = kExpectRequestLine;
        _request.reset(new HttpRequestImpl(_loop));
    }

    // const HttpRequestPtr &request() const
    // {
    //     return _request;
    // }

    const HttpRequestImplPtr &requestImpl() const
    {
        return _request;
    }

    bool firstReq()
    {
        if (_firstRequest)
        {
            _firstRequest = false;
            return true;
        }
        return false;
    }
    const WebSocketConnectionPtr &webSocketConn() const
    {
        return _websockConnPtr;
    }
    void setWebsockConnection(const WebSocketConnectionPtr &conn)
    {
        _websockConnPtr = conn;
    }
    //to support request pipelining(rfc2616-8.1.2.2)
    //std::mutex &getPipeLineMutex();
    void pushRquestToPipeLine(const HttpRequestPtr &req);
    HttpRequestPtr getFirstRequest() const;
    HttpResponsePtr getFirstResponse() const;
    void popFirstRequest();
    void pushResponseToPipeLine(const HttpRequestPtr &req, const HttpResponsePtr &resp);
    size_t numberOfRequestsInPipeLine() const { return _requestPipeLine.size(); }
    bool isStop() const { return _stopWorking; }
    void stop() { _stopWorking = true; }
    size_t numberOfRequestsParsed() const { return _requestsCounter; }

  private:
    bool processRequestLine(const char *begin, const char *end);
    HttpRequestParseState _state;
    trantor::EventLoop *_loop;
    HttpRequestImplPtr _request;
    bool _firstRequest = true;
    WebSocketConnectionPtr _websockConnPtr;
    std::deque<std::pair<HttpRequestPtr, HttpResponsePtr>> _requestPipeLine;
    size_t _requestsCounter = 0;
    std::weak_ptr<trantor::TcpConnection> _conn;
    bool _stopWorking = false;
};

} // namespace drogon
