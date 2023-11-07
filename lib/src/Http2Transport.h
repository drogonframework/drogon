#pragma once

#include "HttpTransport.h"
#include "HttpResponseImpl.h"
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
    HttpReqCallback callback;
    HttpResponseImplPtr response;
    trantor::MsgBuffer body;
};

}  // namespace internal

class Http2Transport : public HttpTransport
{
  private:
    trantor::TcpConnectionPtr connPtr;
    size_t *bytesSent_;
    size_t *bytesReceived_;
    hpack::HPacker hpackTx;
    hpack::HPacker hpackRx;

    std::priority_queue<int32_t> usibleStreamIds;
    int32_t streamIdTop = 1;
    std::unordered_map<int32_t, internal::H2Stream> streams;
    // TODO: Handle server-initiated stream creation

    // HTTP/2 client-wide settings
    size_t maxConcurrentStreams = 100;
    size_t initialWindowSize = 65535;
    size_t maxFrameSize = 16384;
    size_t avaliableWindowSize = 0;

    // Set after server settings are received
    bool serverSettingsReceived = false;
    std::vector<std::pair<HttpRequestPtr, HttpReqCallback>> bufferedRequests;

    int32_t nextStreamId()
    {
        if (usibleStreamIds.empty())
        {
            int32_t id = streamIdTop;
            streamIdTop += 2;
            return id;
        }
        int32_t id = usibleStreamIds.top();
        usibleStreamIds.pop();
        return id;
    }

    void retireStreamId(int32_t id)
    {
        if (id == streamIdTop - 2)
        {
            streamIdTop -= 2;
            while (!usibleStreamIds.empty() &&
                   usibleStreamIds.top() == streamIdTop - 2)
            {
                usibleStreamIds.pop();
                streamIdTop -= 2;
                assert(streamIdTop >= 1);
            }
        }
        else
        {
            usibleStreamIds.push(id);
        }
    }

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

    bool handleConnectionClose() override
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