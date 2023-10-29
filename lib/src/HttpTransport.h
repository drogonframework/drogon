#pragma once

#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <drogon/drogon_callbacks.h>

#include <trantor/net/TcpConnection.h>
#include <trantor/utils/NonCopyable.h>


namespace drogon
{

class HttpTransport : public trantor::NonCopyable
{
  public:
    HttpTransport() = default;
    virtual ~HttpTransport() = default;
    virtual void sendRequestInLoop(const HttpRequestPtr &req,
                                   HttpReqCallback &&callback) = 0;
    virtual void onRecvMessage(const trantor::TcpConnectionPtr &,
                               trantor::MsgBuffer *) = 0;
    virtual bool handleConnectionClose() = 0;
    virtual void onError(ReqResult result) = 0;

    virtual size_t requestsInFlight() const = 0;

    using RespCallback =
        std::function<void(const HttpResponseImplPtr &,
                           std::pair<HttpRequestPtr, HttpReqCallback> &&,
                           const trantor::TcpConnectionPtr)>;

    void setRespCallback(RespCallback cb)
    {
        respCallback = std::move(cb);
    }

    RespCallback respCallback;
};

}