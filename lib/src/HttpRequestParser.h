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

#include <drogon/HttpTypes.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/NonCopyable.h>
#include <deque>
#include <memory>
#include <mutex>
#include "impl_forwards.h"

namespace drogon
{
class HttpRequestParser : public trantor::NonCopyable,
                          public std::enable_shared_from_this<HttpRequestParser>
{
  public:
    enum class HttpRequestParseStatus
    {
        kExpectMethod,
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkLen,
        kExpectChunkBody,
        kExpectLastEmptyChunk,
        kGotAll,
    };

    explicit HttpRequestParser(const trantor::TcpConnectionPtr &connPtr);

    int parseRequest(trantor::MsgBuffer *buf);

    bool gotAll() const
    {
        return status_ == HttpRequestParseStatus::kGotAll;
    }

    void reset();

    const HttpRequestImplPtr &requestImpl() const
    {
        return request_;
    }

    bool firstReq()
    {
        if (firstRequest_)
        {
            firstRequest_ = false;
            return true;
        }
        return false;
    }

    const WebSocketConnectionImplPtr &webSocketConn() const
    {
        return websockConnPtr_;
    }

    void setWebsockConnection(const WebSocketConnectionImplPtr &conn)
    {
        websockConnPtr_ = conn;
    }

    // to support request pipelining(rfc2616-8.1.2.2)
    void pushRequestToPipelining(const HttpRequestPtr &, bool isHeadMethod);
    bool pushResponseToPipelining(const HttpRequestPtr &, HttpResponsePtr);
    void popReadyResponses(std::vector<std::pair<HttpResponsePtr, bool>> &);

    size_t numberOfRequestsInPipelining() const
    {
        return requestPipelining_.size();
    }

    bool emptyPipelining()
    {
        return requestPipelining_.empty();
    }

    bool isStop() const
    {
        return stopWorking_;
    }

    void stop()
    {
        stopWorking_ = true;
    }

    size_t numberOfRequestsParsed() const
    {
        return requestsCounter_;
    }

    trantor::MsgBuffer &getBuffer()
    {
        return sendBuffer_;
    }

    std::vector<std::pair<HttpResponsePtr, bool>> &getResponseBuffer()
    {
        assert(loop_->isInLoopThread());
        if (!responseBuffer_)
        {
            responseBuffer_ =
                std::unique_ptr<std::vector<std::pair<HttpResponsePtr, bool>>>(
                    new std::vector<std::pair<HttpResponsePtr, bool>>);
        }
        return *responseBuffer_;
    }

    std::vector<HttpRequestImplPtr> &getRequestBuffer()
    {
        assert(loop_->isInLoopThread());
        if (!requestBuffer_)
        {
            requestBuffer_ = std::unique_ptr<std::vector<HttpRequestImplPtr>>(
                new std::vector<HttpRequestImplPtr>);
        }
        return *requestBuffer_;
    }

  private:
    HttpRequestImplPtr makeRequestForPool(HttpRequestImpl *p);
    bool processRequestLine(const char *begin, const char *end);
    HttpRequestParseStatus status_;
    trantor::EventLoop *loop_;
    HttpRequestImplPtr request_;
    bool firstRequest_{true};
    WebSocketConnectionImplPtr websockConnPtr_;
    std::deque<std::pair<HttpRequestPtr, std::pair<HttpResponsePtr, bool>>>
        requestPipelining_;
    size_t requestsCounter_{0};
    std::weak_ptr<trantor::TcpConnection> conn_;
    bool stopWorking_{false};
    trantor::MsgBuffer sendBuffer_;
    std::unique_ptr<std::vector<std::pair<HttpResponsePtr, bool>>>
        responseBuffer_;
    std::unique_ptr<std::vector<HttpRequestImplPtr>> requestBuffer_;
    std::vector<HttpRequestImplPtr> requestsPool_;
    size_t currentChunkLength_{0};
    size_t remainContentLength_{0};
};

}  // namespace drogon
