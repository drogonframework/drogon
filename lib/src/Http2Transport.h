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
// Quick and dirty ByteStream implementation and extensions so we can use it
// to read from the buffer, safely. At least it checks for buffer overflows
// in debug mode.
struct ByteStream
{
    ByteStream(uint8_t *ptr, size_t length) : ptr(ptr), length(length)
    {
    }

    ByteStream(const trantor::MsgBuffer &buffer, size_t length)
        : ptr((uint8_t *)buffer.peek()), length(length)
    {
        assert(length <= buffer.readableBytes());
    }

    uint32_t readU24BE()
    {
        assert(length >= 3 && offset <= length - 3);
        uint32_t res =
            ptr[offset] << 16 | ptr[offset + 1] << 8 | ptr[offset + 2];
        offset += 3;
        return res;
    }

    uint32_t readU32BE()
    {
        assert(length >= 4 && offset <= length - 4);
        uint32_t res = ptr[offset] << 24 | ptr[offset + 1] << 16 |
                       ptr[offset + 2] << 8 | ptr[offset + 3];
        offset += 4;
        return res;
    }

    std::pair<bool, int32_t> readBI32BE()
    {
        assert(length >= 4 && offset <= length - 4);
        int32_t res = ptr[offset] << 24 | ptr[offset + 1] << 16 |
                      ptr[offset + 2] << 8 | ptr[offset + 3];
        offset += 4;
        constexpr int32_t mask = 0x7fffffff;
        bool flag = res & (~mask);
        res &= mask;
        return {flag, res};
    }

    uint16_t readU16BE()
    {
        assert(length >= 2 && offset <= length - 2);
        uint16_t res = ptr[offset] << 8 | ptr[offset + 1];
        offset += 2;
        return res;
    }

    uint8_t readU8()
    {
        assert(length >= 1 && offset <= length - 1);
        return ptr[offset++];
    }

    void read(uint8_t *buffer, size_t size)
    {
        assert((length >= size && offset <= length - size) || size == 0);
        memcpy(buffer, ptr + offset, size);
        offset += size;
    }

    void read(std::vector<uint8_t> &buffer, size_t size)
    {
        buffer.resize(buffer.size() + size);
        read(buffer.data(), size);
    }

    std::vector<uint8_t> read(size_t size)
    {
        std::vector<uint8_t> buffer;
        read(buffer, size);
        return buffer;
    }

    void skip(size_t n)
    {
        assert((length >= n && offset <= length - n) || n == 0);
        offset += n;
    }

    size_t size() const
    {
        return length;
    }

    size_t remaining() const
    {
        return length - offset;
    }

  protected:
    uint8_t *ptr;
    size_t length;
    size_t offset = 0;
};

// DITTO but for serialization
struct OByteStream
{
    void writeU24BE(uint32_t value)
    {
        assert(value <= 0xffffff);
        value = htonl(value);
        buffer.append((char *)&value + 1, 3);
    }

    void writeU32BE(uint32_t value)
    {
        value = htonl(value);
        buffer.append((char *)&value, 4);
    }

    void writeU16BE(uint16_t value)
    {
        value = htons(value);
        buffer.append((char *)&value, 2);
    }

    void writeU8(uint8_t value)
    {
        buffer.append((char *)&value, 1);
    }

    void pad(size_t size, uint8_t value = 0)
    {
        buffer.ensureWritableBytes(size);
        auto ptr = (uint8_t *)buffer.peek() + buffer.readableBytes();
        memset(ptr, value, size);
        buffer.hasWritten(size);
    }

    void write(const uint8_t *ptr, size_t size)
    {
        buffer.append((char *)ptr, size);
    }

    void overwriteU24BE(size_t offset, uint32_t value)
    {
        assert(value <= 0xffffff);
        assert(offset <= buffer.readableBytes() - 3);
        assert(buffer.writableBytes() >= 3);
        auto ptr = (uint8_t *)buffer.peek() + offset;
        ptr[0] = value >> 16;
        ptr[1] = value >> 8;
        ptr[2] = value;
    }

    void overwriteU8(size_t offset, uint8_t value)
    {
        assert(offset <= buffer.readableBytes() - 1);
        assert(buffer.writableBytes() >= 1);
        auto ptr = (uint8_t *)buffer.peek() + offset;
        ptr[0] = value;
    }

    uint8_t *peek()
    {
        return (uint8_t *)buffer.peek();
    }

    size_t size() const
    {
        return buffer.readableBytes();
    }

    trantor::MsgBuffer buffer;
};

struct SettingsFrame
{
    SettingsFrame() = default;

    SettingsFrame(bool ack) : ack(ack)
    {
    }

    bool ack = false;
    std::vector<std::pair<uint16_t, uint32_t>> settings;

    static std::optional<SettingsFrame> parse(ByteStream &payload,
                                              uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct WindowUpdateFrame
{
    WindowUpdateFrame() = default;

    WindowUpdateFrame(uint32_t windowSizeIncrement)
        : windowSizeIncrement(windowSizeIncrement)
    {
    }

    uint32_t windowSizeIncrement = 0;

    static std::optional<WindowUpdateFrame> parse(ByteStream &payload,
                                                  uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct HeadersFrame
{
    HeadersFrame() = default;

    HeadersFrame(std::vector<uint8_t> headerBlockFragment,
                 bool endHeaders,
                 bool endStream)
        : headerBlockFragment(std::vector(headerBlockFragment)),
          endHeaders(endHeaders),
          endStream(endStream)
    {
    }

    HeadersFrame(const uint8_t *ptr,
                 size_t size,
                 bool endHeaders,
                 bool endStream)
        : headerBlockFragment(ptr, ptr + size),
          endHeaders(endHeaders),
          endStream(endStream)
    {
    }

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
    GoAwayFrame() = default;

    GoAwayFrame(uint32_t lastStreamId,
                uint32_t errorCode,
                const std::string &additionalDebugData)
        : lastStreamId(lastStreamId),
          errorCode(errorCode),
          additionalDebugData((const uint8_t *)additionalDebugData.data(),
                              (const uint8_t *)additionalDebugData.data() +
                                  additionalDebugData.size())
    {
    }

    uint32_t lastStreamId = 0;
    uint32_t errorCode = 0;
    std::vector<uint8_t> additionalDebugData;

    static std::optional<GoAwayFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct DataFrame
{
    DataFrame() = default;

    DataFrame(std::vector<uint8_t> data, bool endStream)
        : data(std::move(data)), endStream(endStream)
    {
    }

    DataFrame(const uint8_t *ptr, size_t size, bool endStream)
        : data(std::vector<uint8_t>(ptr, ptr + size)), endStream(endStream)
    {
    }

    explicit DataFrame(std::string_view data, bool endStream)
        : data(data), endStream(endStream)
    {
    }

    uint8_t padLength = 0;
    std::variant<std::vector<uint8_t>, std::string_view> data;
    bool endStream = false;

    std::pair<const uint8_t *, size_t> getData() const
    {
        if (std::holds_alternative<std::vector<uint8_t>>(data))
        {
            auto &vec = std::get<std::vector<uint8_t>>(data);
            return {vec.data(), vec.size()};
        }
        else
        {
            auto &str = std::get<std::string_view>(data);
            return {(const uint8_t *)str.data(), str.size()};
        }
    }

    static std::optional<DataFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct PingFrame
{
    PingFrame() = default;

    PingFrame(std::array<uint8_t, 8> opaqueData, bool ack)
        : opaqueData(opaqueData), ack(ack)
    {
    }

    std::array<uint8_t, 8> opaqueData;
    bool ack = false;

    static std::optional<PingFrame> parse(ByteStream &payload, uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct ContinuationFrame
{
    ContinuationFrame() = default;

    ContinuationFrame(std::vector<uint8_t> headerBlockFragment, bool endHeaders)
        : headerBlockFragment(std::move(headerBlockFragment)),
          endHeaders(endHeaders)
    {
    }

    ContinuationFrame(const uint8_t *ptr, size_t size, bool endHeaders)
        : headerBlockFragment(ptr, ptr + size), endHeaders(endHeaders)
    {
    }

    std::vector<uint8_t> headerBlockFragment;
    bool endHeaders = false;

    static std::optional<ContinuationFrame> parse(ByteStream &payload,
                                                  uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct RstStreamFrame
{
    RstStreamFrame() = default;

    RstStreamFrame(uint32_t errorCode) : errorCode(errorCode)
    {
    }

    uint32_t errorCode = 0;

    static std::optional<RstStreamFrame> parse(ByteStream &payload,
                                               uint8_t flags);
    bool serialize(OByteStream &stream, uint8_t &flags) const;
};

struct PushPromiseFrame
{
    PushPromiseFrame() = default;

    PushPromiseFrame(uint32_t promisedStreamId,
                     std::vector<uint8_t> headerBlockFragment,
                     bool endHeaders)
        : promisedStreamId(promisedStreamId),
          headerBlockFragment(std::move(headerBlockFragment)),
          endHeaders(endHeaders)
    {
    }

    uint8_t padLength = 0;
    int32_t promisedStreamId = 0;
    std::vector<uint8_t> headerBlockFragment;
    bool endHeaders = false;

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
    std::string body;
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
    internal::OByteStream batchedSendBuffer;
    int32_t expectngContinuationStreamId = 0;

    std::unordered_map<int32_t, size_t> pendingDataSend;
    bool reconnectionIssued = false;
    // TODO: Handle server-initiated stream creation

    // HTTP/2 client-wide settings (can be changed by server)
    size_t maxConcurrentStreams = 100;
    size_t initialRxWindowSize = 65535;
    size_t initialTxWindowSize = 65535;
    size_t maxFrameSize = 16384;

    // Configuration settings
    const uint32_t windowIncreaseThreshold = 16384;
    const uint32_t windowIncreaseSize = 10 * 1024 * 1024;  // 10 MiB
    const uint32_t maxCompressiedHeaderSize = 2048;
    const int32_t streamIdReconnectThreshold = INT_MAX - 8192;

    // HTTP/2 connection-wide state
    size_t avaliableTxWindow = 65535;
    size_t avaliableRxWindow = 65535;

    internal::H2Stream &createStream(int32_t streamId);
    void responseSuccess(internal::H2Stream &stream);
    void responseErrored(int32_t streamId, ReqResult result);

    std::optional<int32_t> nextStreamId()
    {
        // XXX: Technically UB. But no one acrually uses 1's complement
        if (currentStreamId < 0)
            return std::nullopt;

        int32_t streamId = currentStreamId;
        currentStreamId += 2;
        return streamId;
    }

    // Returns true when we SHOULD reconnect due to exhausting stream IDs.
    // Doesn't mean we will. We will force a reconnect when we actually
    // run out.
    inline bool runningOutStreamId()
    {
        return currentStreamId > streamIdReconnectThreshold;
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
    size_t sendBodyForStream(internal::H2Stream &stream, size_t offset);
    void sendFrame(const internal::H2Frame &frame, int32_t streamId);

    void sendBufferedData();

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
        return streams.size();
    }

    bool handleConnectionClose() override;

    void onError(ReqResult result) override;

  protected:
    void onServerSettingsReceived(){};
};
}  // namespace drogon
