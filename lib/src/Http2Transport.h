#pragma once

#include "HttpTransport.h"

namespace drogon
{
class Http2Transport : public HttpTransport
{
    private:
        trantor::TcpConnectionPtr connPtr;
        size_t *bytesSent_;
        size_t *bytesReceived_;
    public:
    Http2Transport(trantor::TcpConnectionPtr connPtr,
                    size_t *bytesSent,
                    size_t *bytesReceived);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback) override
    {
        // throw std::runtime_error("HTTP/2 sendRequestInLoop not implemented");
    }
    void onRecvMessage(const trantor::TcpConnectionPtr &,
                       trantor::MsgBuffer *) override;

    size_t requestsInFlight() const override
    {
        return 0;
    }

    bool handleConnectionClose()
    {
        throw std::runtime_error("HTTP/2 handleConnectionClose not implemented");
    }

    void onError(ReqResult result) override
    {
        throw std::runtime_error("HTTP/2 onError not implemented");
    }
};
}