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

struct ContinuationFrame
{
    std::vector<uint8_t> headerBlockFragment;
    bool endHeaders = false;

    static std::optional<ContinuationFrame> parse(ByteStream &payload,
                                                  uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct RstStreamFrame
{
    uint32_t errorCode = 0;

    static std::optional<RstStreamFrame> parse(ByteStream &payload,
                                               uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct PushPromiseFrame
{
    uint8_t padLength = 0;
    bool endHeaders = false;
    int32_t promisedStreamId = 0;
    std::vector<uint8_t> headerBlockFragment;

    static std::optional<PushPromiseFrame> parse(ByteStream &payload,
                                                 uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

using H2Frame = std::variant<SettingsFrame,
                             WindowUpdateFrame,
                             HeadersFrame,
                             GoAwayFrame,
                             DataFrame,
                             PingFrame,
                             ContinuationFrame,
                             PushPromiseFrame,
                             RstStreamFrame>;

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

    int32_t currentStreamId = 1;
    std::unordered_map<int32_t, internal::H2Stream> streams;
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> bufferedRequests;
    trantor::MsgBuffer headerBufferRx;
    int32_t expectngContinuationStreamId = 0;

    std::unordered_map<int32_t, size_t> pendingDataSend;
    // TODO: Handle server-initiated stream creation

    // HTTP/2 client-wide settings (can be changed by server)
    size_t maxConcurrentStreams = 100;
    size_t initialWindowSize = 65535;
    size_t maxFrameSize = 16384;

    // Configuration settings
    const uint32_t windowIncreaseThreshold = 16384;
    const uint32_t windowIncreaseSize = 10 * 1024 * 1024;  // 10 MiB
    const uint32_t maxCompressiedHeaderSize = 2048;

    // HTTP/2 connection-wide state
    size_t avaliableTxWindow = 65535;
    size_t avaliableRxWindow = 65535;

    internal::H2Stream &createStream(int32_t streamId);
    void responseSuccess(internal::H2Stream &stream);
    void responseErrored(int32_t streamId, ReqResult result);

    int32_t nextStreamId()
    {
        // TODO: Handling stream ID requires to reconnect
        // the entire connection. Handle this somehow
        int32_t streamId = currentStreamId;
        currentStreamId += 2;
        return streamId;
    }

    void handleFrameForStream(const internal::H2Frame &frame,
                              int32_t streamId,
                              uint8_t flags);
    void killConnection(int32_t lastStreamId,
                        StreamCloseErrorCode errorCode,
                        std::string errorMsg = "");

    bool parseAndApplyHeaders(internal::H2Stream &stream,
                              const void *data,
                              size_t len);

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

    bool handleConnectionClose() override;

    void onError(ReqResult result) override;

  protected:
    void onServerSettingsReceived(){};
};
}  // namespace drogon