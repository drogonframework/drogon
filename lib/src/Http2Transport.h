#pragma once

#include "HttpTransport.h"

namespace drogon
{

namespace internal
{

// Virtual stream that holds properties for the HTTP/2 stream
// Defaults to stream 0 global properties
struct H2Stream
{

};

} // namespace internal
class Http2Transport : public HttpTransport
{
  private:
    trantor::TcpConnectionPtr connPtr;
    size_t *bytesSent_;
    size_t *bytesReceived_;

    // HTTP/2 client-wide settings
    size_t maxConcurrentStreams = 100;
    size_t initialWindowSize = 65535;
    size_t maxFrameSize = 16384;
    size_t avaliableWindowSize = 0;

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
        throw std::runtime_error(
            "HTTP/2 handleConnectionClose not implemented");
    }

    void onError(ReqResult result) override
    {
        throw std::runtime_error("HTTP/2 onError not implemented");
    }
};
}  // namespace drogon