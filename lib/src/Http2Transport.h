#pragma once

#include "HttpTransport.h"
// TOOD: Write our own HPACK implementation
#include "hpack/HPacker.h"

namespace drogon
{

namespace internal
{

// Virtual stream that holds properties for the HTTP/2 stream
// Defaults to stream 0 global properties
struct H2Stream
{
    size_t maxConcurrentStreams = 100;
    size_t initialWindowSize = 65535;
    size_t maxFrameSize = 16384;
    size_t avaliableWindowSize = 0;
};

}  // namespace internal

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

    // Set after server settings are received
    bool serverSettingsReceived = false;
    std::vector<std::pair<HttpRequestPtr, HttpReqCallback>> bufferedRequests;

  public:
    Http2Transport(trantor::TcpConnectionPtr connPtr,
                   size_t *bytesSent,
                   size_t *bytesReceived);

    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback) override;
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

  protected:
    void onServerSettingsReceived(){};
};
}  // namespace drogon