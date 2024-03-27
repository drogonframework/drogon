#pragma once

#include <trantor/net/EventLoop.h>
#include <trantor/net/TcpClient.h>
#include <list>
#include <queue>
#include <vector>
#include "HttpTransport.h"

namespace drogon
{
class Http1xTransport : public HttpTransport
{
  private:
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> pipeliningCallbacks_;
    trantor::TcpConnectionPtr connPtr;
    size_t *bytesSent_;
    size_t *bytesReceived_;
    Version version_{Version::kHttp11};

    void sendReq(const HttpRequestPtr &req);

  public:
    Http1xTransport(trantor::TcpConnectionPtr connPtr,
                    Version version,
                    size_t *bytesSent,
                    size_t *bytesReceived);
    virtual ~Http1xTransport();
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback) override;
    void onRecvMessage(const trantor::TcpConnectionPtr &,
                       trantor::MsgBuffer *) override;

    size_t requestsInFlight() const override
    {
        return pipeliningCallbacks_.size();
    }

    bool handleConnectionClose() override;

    void onError(ReqResult result) override
    {
        while (!pipeliningCallbacks_.empty())
        {
            auto &cb = pipeliningCallbacks_.front().second;
            cb(result, nullptr);
            pipeliningCallbacks_.pop();
        }
    }
};

}  // namespace drogon
