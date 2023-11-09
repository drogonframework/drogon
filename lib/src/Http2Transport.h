#pragma once

#include "HttpTransport.h"
#include "HttpResponseImpl.h"
// TOOD: Write our own HPACK implementation
#include "hpack/HPacker.h"

#include <variant>

namespace drogon
{

namespace internal
{
struct ByteStream;
struct OByteStream;

struct SettingsFrame
{
    bool ack = false;
    std::vector<std::pair<uint16_t, uint32_t>> settings;

    static std::optional<SettingsFrame> parse(ByteStream &payload,
                                              uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct WindowUpdateFrame
{
    uint32_t windowSizeIncrement = 0;

    static std::optional<WindowUpdateFrame> parse(ByteStream &payload,
                                                  uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct HeadersFrame
{
    uint8_t padLength = 0;
    bool exclusive = false;
    uint32_t streamDependency = 0;
    uint8_t weight = 0;
    std::vector<uint8_t> headerBlockFragment;
    bool endHeaders = false;
    bool endStream = false;

    static std::optional<HeadersFrame> parse(ByteStream &payload,
                                             uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct GoAwayFrame
{
    uint32_t lastStreamId = 0;
    uint32_t errorCode = 0;
    std::vector<uint8_t> additionalDebugData;

    static std::optional<GoAwayFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct DataFrame
{
    uint8_t padLength = 0;
    std::vector<uint8_t> data;
    bool endStream = false;

    static std::optional<DataFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct PingFrame
{
    std::array<uint8_t, 8> opaqueData;
    bool ack = false;

    static std::optional<PingFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

using H2Frame = std::variant<SettingsFrame,
                             WindowUpdateFrame,
                             HeadersFrame,
                             GoAwayFrame,
                             DataFrame,
                             PingFrame>;

enum class StreamState
{
    SendingBody,
    ExpectingHeaders,
    ExpectingContinuation,
    ExpectingData,
    Finished,
};

// Virtual stream that holds properties for the HTTP/2 stream
// Defaults to stream 0 global properties
struct H2Stream
{
    HttpReqCallback callback;
    HttpResponseImplPtr response;
    HttpRequestPtr request;
    trantor::MsgBuffer body;
    std::optional<size_t> contentLength;
    int32_t streamId = 0;
    size_t avaliableTxWindow = 65535;
    size_t avaliableRxWindow = 65535;
    StreamState state = StreamState::ExpectingHeaders;
};
}  // namespace internal

enum class StreamCloseErrorCode
{
    NoError = 0x0,
    ProtocolError = 0x1,
    InternalError = 0x2,
    FlowControlError = 0x3,
    SettingsTimeout = 0x4,
    StreamClosed = 0x5,
    FrameSizeError = 0x6,
    RefusedStream = 0x7,
    Cancel = 0x8,
    CompressionError = 0x9,
    ConnectError = 0xa,
    EnhanceYourCalm = 0xb,
    InadequateSecurity = 0xc,
    Http11Required = 0xd,
};

class Http2Transport : public HttpTransport
{
  private:
    // Implementation details, stuff we need to implement HTTP/2
    trantor::TcpConnectionPtr connPtr;
    size_t *bytesSent_;
    size_t *bytesReceived_;
    hpack::HPacker hpackTx;
    hpack::HPacker hpackRx;

    std::priority_queue<int32_t> usibleStreamIds;
    int32_t streamIdTop = 1;
    std::unordered_map<int32_t, internal::H2Stream> streams;
    bool serverSettingsReceived = false;
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> bufferedRequests;
    // TODO: Handle server-initiated stream creation

    // HTTP/2 client-wide settings (can be changed by server)
    size_t maxConcurrentStreams = 100;
    size_t initialWindowSize = 65535;
    size_t maxFrameSize = 16384;

    // Configuration settings
    const uint32_t windowIncreaseThreshold = 16384;
    const uint32_t windowIncreaseSize = 32 * 1024 * 1024;  // 10 MiB
    const uint32_t maxCompressiedHeaderSize = 2048;

    // HTTP/2 connection-wide state
    size_t avaliableTxWindow = 65535;
    size_t avaliableRxWindow = 65535;

    internal::H2Stream &createStream(int32_t streamId);
    void streamFinished(internal::H2Stream &stream);
    void streamFinished(int32_t streamId,
                        ReqResult result,
                        StreamCloseErrorCode errorCode,
                        std::string errorMsg = "");

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

    void handleFrameForStream(const internal::H2Frame &frame,
                              int32_t streamId,
                              uint8_t flags);
    void killConnection(int32_t lastStreamId,
                        StreamCloseErrorCode errorCode,
                        std::string errorMsg = "");

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

    void onError(ReqResult result) override;

  protected:
    void onServerSettingsReceived(){};
};
}  // namespace drogon