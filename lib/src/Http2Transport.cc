#include "Http2Transport.h"
#include "HttpFileUploadRequest.h"

#include <fstream>
#include <variant>

using namespace drogon;
using namespace drogon::internal;

static const std::string_view h2_preamble = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

static std::optional<size_t> stosz(const std::string &str)
{
    try
    {
        size_t idx = 0;
        size_t res = std::stoull(str, &idx);
        if (idx != str.size())
            return std::nullopt;
        return res;
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

enum class H2FrameType
{
    Data = 0x0,
    Headers = 0x1,
    Priority = 0x2,
    RstStream = 0x3,
    Settings = 0x4,
    PushPromise = 0x5,
    Ping = 0x6,
    GoAway = 0x7,
    WindowUpdate = 0x8,
    Continuation = 0x9,
    AltSvc = 0xa,
    // UNUSED = 0xb, // 0xb is removed from the spec
    Origin = 0xc,

    NumEntries
};

enum class H2SettingsKey
{
    HeaderTableSize = 0x1,
    EnablePush = 0x2,
    MaxConcurrentStreams = 0x3,
    InitialWindowSize = 0x4,
    MaxFrameSize = 0x5,
    MaxHeaderListSize = 0x6,

    NumEntries
};

enum class H2HeadersFlags
{
    EndStream = 0x1,
    EndHeaders = 0x4,
    Padded = 0x8,
    Priority = 0x20
};

enum class H2DataFlags
{
    EndStream = 0x1,
    Padded = 0x8
};

enum class H2PingFlags
{
    Ack = 0x1
};

namespace drogon::internal
{
template <typename T>
struct Defer
{
    Defer(T &&f) : f(std::move(f))
    {
    }

    ~Defer()
    {
        f();
    }

    T f;
};

std::optional<SettingsFrame> SettingsFrame::parse(ByteStream &payload,
                                                  uint8_t flags)
{
    if (payload.size() % 6 != 0)
    {
        LOG_TRACE << "Invalid settings frame length";
        return std::nullopt;
    }

    SettingsFrame frame;
    if ((flags & 0x1) != 0)
    {
        frame.ack = true;
        if (payload.size() != 0)
        {
            LOG_TRACE << "Settings frame with ACK flag set should have "
                         "empty payload";
            return std::nullopt;
        }
        return frame;
    }

    for (size_t i = 0; i < payload.size(); i += 6)
    {
        uint16_t key = payload.readU16BE();
        uint32_t value = payload.readU32BE();
        frame.settings.emplace_back(key, value);
    }
    return frame;
}

bool SettingsFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = (ack ? 0x1 : 0x0);
    for (auto &[key, value] : settings)
    {
        stream.writeU16BE(key);
        stream.writeU32BE(value);
    }
    return true;
}

std::optional<WindowUpdateFrame> WindowUpdateFrame::parse(ByteStream &payload,
                                                          uint8_t flags)
{
    if (payload.size() != 4)
    {
        LOG_TRACE << "Invalid window update frame length";
        return std::nullopt;
    }
    WindowUpdateFrame frame;
    // MSB is reserved for future use
    auto [_, windowSizeIncrement] = payload.readBI32BE();
    frame.windowSizeIncrement = windowSizeIncrement;
    return frame;
}

bool WindowUpdateFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    if (windowSizeIncrement & (1U << 31))
    {
        LOG_TRACE << "MSB of windowSizeIncrement should be 0";
        return false;
    }
    stream.writeU32BE(windowSizeIncrement);
    return true;
}

std::optional<HeadersFrame> HeadersFrame::parse(ByteStream &payload,
                                                uint8_t flags)
{
    bool endStream = flags & (uint8_t)H2HeadersFlags::EndStream;
    bool endHeaders = flags & (uint8_t)H2HeadersFlags::EndHeaders;
    bool padded = flags & (uint8_t)H2HeadersFlags::Padded;
    bool priority = flags & (uint8_t)H2HeadersFlags::Priority;

    if (payload.size() == 0)
    {
        LOG_TRACE << "Header size cannot be 0";
        return std::nullopt;
    }

    HeadersFrame frame;
    if (padded)
    {
        frame.padLength = payload.readU8();
    }

    size_t minSize = frame.padLength + (priority ? 5 : 0);
    if (payload.size() < minSize)
    {
        LOG_TRACE << "Invalid headers frame length";
        return std::nullopt;
    }

    if (priority)
    {
        auto [exclusive, streamDependency] = payload.readBI32BE();
        frame.exclusive = exclusive;
        frame.streamDependency = streamDependency;
        frame.weight = payload.readU8();
    }

    frame.endHeaders = endHeaders;
    frame.endStream = endStream;

    assert(payload.remaining() >= frame.padLength);
    int64_t payloadSize = payload.remaining() - frame.padLength;
    if (payloadSize < 0)
    {
        LOG_TRACE << "headers padding is larger than the payload size";
        return std::nullopt;
    }
    frame.headerBlockFragment.resize(payloadSize);
    payload.read(frame.headerBlockFragment.data(),
                 frame.headerBlockFragment.size());
    payload.skip(frame.padLength);
    return frame;
}

bool HeadersFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    if (padLength > 0)
    {
        flags |= (uint8_t)H2HeadersFlags::Padded;
        stream.writeU8(padLength);
    }
    if (exclusive)
    {
        flags |= (uint8_t)H2HeadersFlags::Priority;
        uint32_t streamDependency = this->streamDependency;
        if (exclusive)
            streamDependency |= 1U << 31;
        stream.writeU32BE(streamDependency);
        stream.writeU8(weight);
    }

    if (endHeaders)
        flags |= (uint8_t)H2HeadersFlags::EndHeaders;
    if (endStream)
        flags |= (uint8_t)H2HeadersFlags::EndStream;
    stream.write(headerBlockFragment.data(), headerBlockFragment.size());
    return true;
}

std::optional<GoAwayFrame> GoAwayFrame::parse(ByteStream &payload,
                                              uint8_t flags)
{
    if (payload.size() < 8)
    {
        LOG_TRACE << "Invalid go away frame length";
        return std::nullopt;
    }
    GoAwayFrame frame;
    frame.lastStreamId = payload.readU32BE();
    frame.errorCode = payload.readU32BE();
    frame.additionalDebugData.resize(payload.remaining());
    for (size_t i = 0; i < frame.additionalDebugData.size(); ++i)
        frame.additionalDebugData[i] = payload.readU8();
    return frame;
}

bool GoAwayFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    stream.writeU32BE(lastStreamId);
    stream.writeU32BE(errorCode);
    stream.write(additionalDebugData.data(), additionalDebugData.size());
    return true;
}

std::optional<DataFrame> DataFrame::parse(ByteStream &payload, uint8_t flags)
{
    bool endStream = flags & (uint8_t)H2DataFlags::EndStream;
    bool padded = flags & (uint8_t)H2DataFlags::Padded;

    DataFrame frame;
    if (padded)
    {
        frame.padLength = payload.readU8();
    }

    size_t minSize = frame.padLength;
    if (payload.remaining() < minSize)
    {
        LOG_TRACE << "Invalid data frame length";
        return std::nullopt;
    }

    if (endStream)
    {
        frame.endStream = true;
    }

    assert(payload.remaining() >= frame.padLength);
    size_t payloadSize = payload.remaining() - frame.padLength;
    if (payloadSize < 0)
    {
        LOG_TRACE << "data padding is larger than the payload size";
        return std::nullopt;
    }

    if (payloadSize > 0x7fffffff)
    {
        LOG_ERROR << "data frame payload size too large";
        return std::nullopt;
    }

    std::vector<uint8_t> data;
    data.resize(payloadSize);
    payload.read(data.data(), data.size());
    payload.skip(frame.padLength);
    frame.data = std::move(data);
    return frame;
}

bool DataFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = (endStream ? (uint8_t)H2DataFlags::EndStream : 0x0);
    auto [ptr, size] = getData();

    stream.write(ptr, size);
    if (padLength > 0)
    {
        flags |= (uint8_t)H2DataFlags::Padded;
        stream.pad(padLength, 0);
    }
    return true;
}

std::optional<PingFrame> PingFrame::parse(ByteStream &payload, uint8_t flags)
{
    if (payload.size() != 8)
    {
        LOG_TRACE << "Invalid ping frame length";
        return std::nullopt;
    }
    PingFrame frame;
    payload.read(frame.opaqueData.data(), frame.opaqueData.size());
    return frame;
}

bool PingFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = ack ? (uint8_t)H2PingFlags::Ack : 0x0;
    stream.write(opaqueData.data(), opaqueData.size());
    return true;
}

std::optional<ContinuationFrame> ContinuationFrame::parse(ByteStream &payload,
                                                          uint8_t flags)
{
    bool endHeaders = flags & (uint8_t)H2HeadersFlags::EndHeaders;
    ContinuationFrame frame;
    if (endHeaders)
    {
        frame.endHeaders = true;
    }

    frame.headerBlockFragment.resize(payload.remaining());
    payload.read(frame.headerBlockFragment.data(),
                 frame.headerBlockFragment.size());
    return frame;
}

bool ContinuationFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    if (endHeaders)
        flags |= (uint8_t)H2HeadersFlags::EndHeaders;
    stream.write(headerBlockFragment.data(), headerBlockFragment.size());
    return true;
}

std::optional<PushPromiseFrame> PushPromiseFrame::parse(ByteStream &payload,
                                                        uint8_t flags)
{
    bool endHeaders = flags & (uint8_t)H2HeadersFlags::EndHeaders;
    bool padded = flags & (uint8_t)H2HeadersFlags::Padded;

    PushPromiseFrame frame;
    if (padded)
        frame.padLength = payload.readU8();
    if (endHeaders)
        frame.endHeaders = true;

    size_t minSize = frame.padLength + 4;
    if (payload.size() < minSize)
    {
        LOG_TRACE << "Invalid push promise frame length";
        return std::nullopt;
    }

    auto [_, promisedStreamId] = payload.readBI32BE();
    frame.promisedStreamId = promisedStreamId;
    assert(payload.remaining() >= frame.padLength);
    frame.headerBlockFragment.resize(payload.remaining() - frame.padLength);
    payload.read(frame.headerBlockFragment.data(),
                 frame.headerBlockFragment.size());
    payload.skip(frame.padLength);
    return frame;
}

bool PushPromiseFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    if (endHeaders)
        flags |= (uint8_t)H2HeadersFlags::EndHeaders;
    assert(promisedStreamId > 0);
    stream.writeU32BE(promisedStreamId);
    stream.write(headerBlockFragment.data(), headerBlockFragment.size());
    if (padLength > 0)
    {
        flags |= (uint8_t)H2HeadersFlags::Padded;
        stream.writeU8(padLength);
    }
    return true;
}

std::optional<RstStreamFrame> RstStreamFrame::parse(ByteStream &payload,
                                                    uint8_t flags)
{
    if (payload.size() != 4)
    {
        LOG_TRACE << "Invalid RST_STREAM frame length";
        return std::nullopt;
    }
    RstStreamFrame frame;
    frame.errorCode = payload.readU32BE();
    return frame;
}

bool RstStreamFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    stream.writeU32BE(errorCode);
    return true;
}
}  // namespace drogon::internal

// Print the HEX and ASCII representation of the buffer side by side
// 16 bytes per line. Same function as the xdd command in linux.
static std::string dump_hex_beautiful(const void *ptr, size_t size)
{
    std::stringstream ss;
    ss << "\n";
    for (size_t i = 0; i < size; i += 16)
    {
        ss << std::setw(8) << std::setfill('0') << std::hex << i << ": ";
        for (size_t j = 0; j < 16; ++j)
        {
            if (i + j < size)
            {
                ss << std::setw(2) << std::setfill('0') << std::hex
                   << (int)((unsigned char *)ptr)[i + j] << " ";
            }
            else
            {
                ss << "   ";
            }
        }
        ss << " ";
        for (size_t j = 0; j < 16; ++j)
        {
            if (i + j < size)
            {
                if (((unsigned char *)ptr)[i + j] >= 32 &&
                    ((unsigned char *)ptr)[i + j] < 127)
                {
                    ss << (char)((unsigned char *)ptr)[i + j];
                }
                else
                {
                    ss << ".";
                }
            }
        }
        ss << "\n";
    }
    return ss.str();
}

static void serializeFrame(OByteStream &buffer,
                           const H2Frame &frame,
                           int32_t streamId)
{
    size_t baseOffset = buffer.size();
    buffer.writeU24BE(0);  // Placeholder for length
    buffer.writeU8(0);     // Placeholder for type
    buffer.writeU8(0);     // Placeholder for flags
    buffer.writeU32BE(streamId);

    uint8_t type;
    uint8_t flags;
    bool ok = false;
    if (std::holds_alternative<HeadersFrame>(frame))
    {
        const auto &f = std::get<HeadersFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::Headers;
    }
    else if (std::holds_alternative<SettingsFrame>(frame))
    {
        const auto &f = std::get<SettingsFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::Settings;
    }
    else if (std::holds_alternative<WindowUpdateFrame>(frame))
    {
        const auto &f = std::get<WindowUpdateFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::WindowUpdate;
    }
    else if (std::holds_alternative<GoAwayFrame>(frame))
    {
        const auto &f = std::get<GoAwayFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::GoAway;
    }
    else if (std::holds_alternative<DataFrame>(frame))
    {
        const auto &f = std::get<DataFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::Data;
    }
    else if (std::holds_alternative<PingFrame>(frame))
    {
        const auto &f = std::get<PingFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::Ping;
    }
    else if (std::holds_alternative<ContinuationFrame>(frame))
    {
        const auto &f = std::get<ContinuationFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::Continuation;
    }
    else if (std::holds_alternative<PushPromiseFrame>(frame))
    {
        const auto &f = std::get<PushPromiseFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::PushPromise;
    }
    else if (std::holds_alternative<RstStreamFrame>(frame))
    {
        const auto &f = std::get<RstStreamFrame>(frame);
        ok = f.serialize(buffer, flags);
        type = (uint8_t)H2FrameType::RstStream;
    }

    if (!ok)
    {
        LOG_ERROR << "Failed to serialize frame";
        abort();
    }

    auto length = buffer.buffer.readableBytes() - 9 - baseOffset;
    if (length > 0x7fffff)
    {
        LOG_FATAL << "HTTP/2 frame too large during serialization";
        abort();
    }
    buffer.overwriteU24BE(baseOffset + 0, (int)length);
    buffer.overwriteU8(baseOffset + 3, type);
    buffer.overwriteU8(baseOffset + 4, flags);
}

// return data {frame, streamId, h2flags, fatal-error}
// Note that error can orrcur on a stream level or the entire connection
// We need to handle both cases. Also it could happen that the TCP stream
// just cuts off in the middle of a frame (or header). We need to handle that
// too.
static std::tuple<std::optional<H2Frame>, uint32_t, uint8_t, bool> parseH2Frame(
    trantor::MsgBuffer *msg)
{
    if (msg->readableBytes() < 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame header";
        return {std::nullopt, 0, 0, false};
    }

    uint8_t *ptr = (uint8_t *)msg->peek();
    ByteStream header(ptr, 9);

    // 24 bits length
    const uint32_t length = header.readU24BE();
    if (msg->readableBytes() < length + 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame";
        return {std::nullopt, 0, 0, false};
    }

    const uint8_t type = header.readU8();
    const uint8_t flags = header.readU8();
    // MSB is reserved for future use
    const uint32_t streamId = header.readU32BE() & ((1U << 31) - 1);

    if (type >= (uint8_t)H2FrameType::NumEntries)
    {
        // TODO: Handle fatal protocol error
        LOG_TRACE << "Unsupported H2 frame type: " << (int)type;
        return {std::nullopt, streamId, 0, true};
    }

    LOG_TRACE << "H2 frame: length=" << length << " type=" << (int)type
              << " flags=" << (int)flags << " streamId=" << streamId;

    ByteStream payload(ptr + 9, length);
    std::optional<H2Frame> frame;
    if (type == (uint8_t)H2FrameType::Settings)
        frame = SettingsFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::WindowUpdate)
        frame = WindowUpdateFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::GoAway)
        frame = GoAwayFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::Headers)
        frame = HeadersFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::Data)
        frame = DataFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::Ping)
        frame = PingFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::Continuation)
        frame = ContinuationFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::PushPromise)
        frame = PushPromiseFrame::parse(payload, flags);
    else if (type == (uint8_t)H2FrameType::RstStream)
        frame = RstStreamFrame::parse(payload, flags);
    else
    {
        LOG_WARN << "Unsupported H2 frame type: " << (int)type;
        msg->retrieve(length + 9);
        return {std::nullopt, streamId, 0, false};
    }

    if (payload.remaining() != 0)
        LOG_WARN << "Invalid H2 frame payload length or bug in parsing!!";

    msg->retrieve(length + 9);
    if (!frame)
    {
        LOG_TRACE << "Failed to parse H2 frame of type: " << (int)type;
        return {std::nullopt, streamId, 0, true};
    }

    return {frame, streamId, flags, false};
}

void Http2Transport::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback)
{
    connPtr->getLoop()->assertInLoopThread();
    Defer d([this]() { sendBufferedData(); });
    if (streams.size() + 1 >= maxConcurrentStreams)
    {
        LOG_TRACE << "Too many streams in flight. Buffering request";
        bufferedRequests.push({req, std::move(callback)});
        return;
    }

    const auto sid = nextStreamId();
    if (!sid.has_value())
    {
        // Upper HTTP client should see this and retry
        // TODO: Need more elegant handling
        connPtr->forceClose();
        return;
    }
    const auto streamId = *sid;
    assert(streamId % 2 == 1);
    LOG_TRACE << "Sending HTTP/2 request: streamId=" << streamId;
    if (streams.find(streamId) != streams.end())
    {
        LOG_FATAL << "Stream id already in use! This should not happen";
        killConnection(streamId,
                       StreamCloseErrorCode::InternalError,
                       "Internal stream id conflict");
        return;
    }

    const auto &headers = req->headers();

    LOG_TRACE << "Sending HTTP/2 headers: size=" << headers.size();
    hpack::HPacker::KeyValueVector vec;
    vec.reserve(headers.size() + 5);
    vec.emplace_back(":method", req->methodString());
    vec.emplace_back(":path", req->path());
    vec.emplace_back(":scheme", connPtr->isSSLConnection() ? "https" : "http");
    if (auto host = req->getHeader("host"); !host.empty())
        vec.emplace_back(":authority", host);

    constexpr std::array<std::string_view, 2> skip = {"host", "connection"};
    for (auto &[key, value] : headers)
    {
        if (std::find(skip.begin(), skip.end(), key) != skip.end())
            continue;

        vec.emplace_back(key, value);
    }

    LOG_TRACE << "Final headers size: " << vec.size();
    for (auto &[key, value] : vec)
        LOG_TRACE << "  " << key << ": " << value;
    std::vector<uint8_t> encoded(maxCompressiedHeaderSize);
    int n = hpackTx.encode(vec, encoded.data(), encoded.size());
    if (n < 0)
    {
        LOG_TRACE << "Failed to encode headers. Internal error or header "
                     "block too large";
        killConnection(streamId,
                       StreamCloseErrorCode::InternalError,
                       "Internal error or header block too large");
        return;
    }
    encoded.resize(n);
    LOG_TRACE << "Encoded headers size: " << encoded.size();

    bool haveBody =
        req->body().length() > 0 ||
        (req->contentType() != CT_MULTIPART_FORM_DATA &&
         dynamic_cast<HttpFileUploadRequest *>(req.get()) != nullptr);
    auto &stream = createStream(streamId);
    stream.callback = std::move(callback);
    stream.request = req;
    bool needsContinuation = encoded.size() > maxFrameSize;
    for (size_t i = 0; i < encoded.size(); i += maxFrameSize)
    {
        bool isFirst = i == 0;
        bool isLast = i + maxFrameSize >= encoded.size();
        size_t dataSize = (std::min)(maxFrameSize, encoded.size() - i);
        uint8_t *data = encoded.data() + i;

        auto frame = [&]() -> H2Frame {
            if (isFirst)
                return HeadersFrame(data,
                                    dataSize,
                                    isLast,
                                    !haveBody && isLast);
            return ContinuationFrame(data, dataSize, isLast);
        }();

        sendFrame(frame, streamId);
    }
    if (needsContinuation && !haveBody)
    {
        DataFrame frame(std::vector<uint8_t>(), true);
        sendFrame(frame, streamId);
        return;
    }

    if (!haveBody)
        return;

    auto [sentOffset, done] = sendBodyForStream(stream, 0);
    if (!done)
    {
        auto it = pendingDataSend.find(streamId);
        if (it != pendingDataSend.end())
        {
            LOG_FATAL << "Stream id already in use! This should not happen";
            abort();
        }

        pendingDataSend.emplace(streamId, sentOffset);
    }
}

void Http2Transport::onRecvMessage(const trantor::TcpConnectionPtr &,
                                   trantor::MsgBuffer *msg)
{
    Defer d([this]() { sendBufferedData(); });
    LOG_TRACE << "HTTP/2 message received:";
    assert(bytesReceived_ != nullptr);
    *bytesReceived_ += msg->readableBytes();
    LOG_TRACE << dump_hex_beautiful(msg->peek(), msg->readableBytes());
    while (true)
    {
        if (avaliableRxWindow < windowIncreaseThreshold)
        {
            WindowUpdateFrame windowUpdateFrame(windowIncreaseSize);
            sendFrame(windowUpdateFrame, 0);
            avaliableRxWindow += windowIncreaseSize;
        }

        if (msg->readableBytes() == 0)
            break;

        auto [frameOpt, streamId, flags, error] = parseH2Frame(msg);
        if (error)
        {
            LOG_TRACE << "Fatal protocol error happened during stream parsing";
            killConnection(
                streamId,
                StreamCloseErrorCode::ProtocolError,
                "Fatal protocol error happened during stream parsing");
            return;
        }
        else if (frameOpt.has_value() == false)
            break;  // not enough data. Wait for more

        assert(frameOpt.has_value());
        auto &frame = *frameOpt;

        // special case for PING and GOAWAY. These are all global frames
        if (std::holds_alternative<GoAwayFrame>(frame))
        {
            auto &f = std::get<GoAwayFrame>(frame);
            if (f.lastStreamId == currentStreamId - 2 && streams.size() == 0 &&
                f.errorCode == 0)
            {
                LOG_TRACE << "Go away frame received. Gracefully shutdown";
                connPtr->shutdown();
                return;
            }

            for (auto &[streamId, stream] : streams)
            {
                if (streamId > f.lastStreamId)
                {
                    responseErrored(streamId, ReqResult::BadResponse);
                }
            }
            // TODO: Should be half-closed but trantor doesn't support it
            connPtr->shutdown();
        }
        else if (std::holds_alternative<PingFrame>(frame))
        {
            auto &f = std::get<PingFrame>(frame);
            if (f.ack)
            {
                LOG_TRACE << "Ping frame received with ACK flag set";
                continue;
            }
            LOG_TRACE << "Ping frame received. Sending ACK";
            PingFrame ackFrame(f.opaqueData, true);
            sendFrame(ackFrame, 0);
            continue;
        }

        // If we are expecting a CONTINUATION frame, we should not receive
        // HEADERS or CONTINUATION from other streams
        if (expectngContinuationStreamId != 0 &&
            (std::holds_alternative<HeadersFrame>(frame) ||
             (std::holds_alternative<ContinuationFrame>(frame) &&
              streamId != expectngContinuationStreamId)))
        {
            LOG_TRACE << "Protocol error: unexpected HEADERS or "
                         "CONTINUATION frame";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "Expecting CONTINUATION frame for stream " +
                               std::to_string(expectngContinuationStreamId));
            break;
        }

        if (std::holds_alternative<PushPromiseFrame>(frame))
        {
            LOG_TRACE << "Push promise frame received. Not supported yet. "
                         "Connection will die";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "Push promise not supported");
            break;
        }

        if (streamId != 0)
        {
            handleFrameForStream(frame, streamId, flags);
            continue;
        }

        // This point forawrd, we are handling frames for stream 0
        if (std::holds_alternative<WindowUpdateFrame>(frame))
        {
            auto &f = std::get<WindowUpdateFrame>(frame);
            avaliableTxWindow += f.windowSizeIncrement;

            auto it = pendingDataSend.begin();
            if (it == pendingDataSend.end())
                continue;

            // HACK: Not so efficient round-robin to send all pending data
            // if we can
            do
            {
                auto &stream = streams[it->first];
                auto [sentOffset, done] = sendBodyForStream(stream, it->second);
                if (done)
                {
                    pendingDataSend.erase(it);
                    it = pendingDataSend.begin();
                    continue;
                }
                it->second = sentOffset;
                if (avaliableTxWindow != 0)
                    ++it;
                else
                    break;
            } while (it != pendingDataSend.end());
        }
        else if (std::holds_alternative<SettingsFrame>(frame))
        {
            auto &f = std::get<SettingsFrame>(frame);
            for (auto &[key, value] : f.settings)
            {
                if (key == (uint16_t)H2SettingsKey::HeaderTableSize)
                {
                    hpackRx.setMaxTableSize(value);
                }
                else if (key == (uint16_t)H2SettingsKey::MaxConcurrentStreams)
                {
                    // Note: MAX_CONCURRENT_STREAMS can be 0, which means
                    // the client is not allowed to send any request. I doubt
                    // much client obey this rule though.
                    maxConcurrentStreams = value;
                }
                else if (key == (uint16_t)H2SettingsKey::MaxFrameSize)
                {
                    if (value < 16384 || value > 16777215)
                    {
                        killConnection(streamId,
                                       StreamCloseErrorCode::ProtocolError,
                                       "MaxFrameSize must be between 16384 and "
                                       "16777215");
                        break;
                    }
                    maxFrameSize = value;
                }
                else if (key == (uint16_t)H2SettingsKey::InitialWindowSize)
                {
                    if (value > 0x7fffffff)
                    {
                        killConnection(streamId,
                                       StreamCloseErrorCode::FlowControlError,
                                       "InitialWindowSize too large");
                        break;
                    }
                    initialTxWindowSize = value;
                }
                else if (key == (uint16_t)H2SettingsKey::MaxHeaderListSize)
                {
                    // no-op. we don't care
                }
                else
                {
                    LOG_TRACE << "Unsupported settings key: " << key;
                }
            }
            // Somehow nghttp2 wants us to send ACK after sending our
            // preferences??
            if (flags == 1)
                continue;
            LOG_TRACE << "Acknowledge settings frame";
            SettingsFrame ackFrame(true);
            sendFrame(ackFrame, streamId);
        }
        else if (std::holds_alternative<HeadersFrame>(frame))
        {
            // Should never show up on stream 0
            LOG_FATAL << "Protocol error: HEADERS frame on stream 0";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "HEADERS frame on stream 0");
            break;
        }
        else if (std::holds_alternative<DataFrame>(frame))
        {
            LOG_FATAL << "Protocol error: DATA frame on stream 0";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "DATA frame on stream 0");
            break;
        }
        else if (std::holds_alternative<ContinuationFrame>(frame))
        {
            LOG_FATAL << "Protocol error: CONTINUATION frame on stream 0";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "CONTINUATION frame on stream 0");
            break;
        }
        else if (std::holds_alternative<RstStreamFrame>(frame))
        {
            LOG_FATAL << "Protocol error: RST_STREAM frame on stream 0";
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "RST_STREAM frame on stream 0");
            break;
        }
        else
        {
            // Do nothing. RFC says to ignore unknown frames
        }
    }

    if (!runningOutStreamId() || reconnectionIssued)
        return;

    // Only attempt to reconnect if there is no request in progress
    if (requestsInFlight() == 0 && bufferedRequests.empty())
    {
        reconnectionIssued = true;
        LOG_TRACE << "Reconnect due to running out of stream ids.";
        connPtr->getLoop()->queueInLoop(
            [connPtr = this->connPtr]() { connPtr->shutdown(); });
    }
}

bool Http2Transport::parseAndApplyHeaders(internal::H2Stream &stream,
                                          const void *data,
                                          size_t size)
{
    hpack::HPacker::KeyValueVector headers;
    int n = hpackRx.decode((const uint8_t *)data, size, headers);
    auto streamId = stream.streamId;
    if (n < 0)
    {
        LOG_TRACE << "Failed to decode headers";
        killConnection(streamId,
                       StreamCloseErrorCode::CompressionError,
                       "Failed to decode headers");
        return false;
    }
    for (auto &[key, value] : headers)
        LOG_TRACE << "  " << key << ": " << value;
    assert(stream.response == nullptr);
    stream.response = std::make_shared<HttpResponseImpl>();
    stream.response->setVersion(Version::kHttp2);
    for (const auto &[key, value] : headers)
    {
        // TODO: Filter more pseudo headers
        if (key == "content-length")
        {
            auto sz = stosz(value);
            if (!sz)
            {
                responseErrored(streamId, ReqResult::BadResponse);
                return false;
            }
            stream.contentLength = std::move(sz);
        }
        else if (key == ":status")
        {
            auto status = stosz(value);
            if (!status)
            {
                responseErrored(streamId, ReqResult::BadResponse);
                return false;
            }
            stream.response->setStatusCode((HttpStatusCode)*status);
            continue;
        }

        // Anti request smuggling. We look for \r or \n in the header
        // name or value. If we find one, we abort the stream.
        if (key.find_first_of("\r\n") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos)
        {
            responseErrored(streamId, ReqResult::BadResponse);
            return false;
        }

        stream.response->addHeader(key, value);
    }
    return true;
}

Http2Transport::Http2Transport(trantor::TcpConnectionPtr connPtr,
                               size_t *bytesSent,
                               size_t *bytesReceived)
    : connPtr(connPtr), bytesSent_(bytesSent), bytesReceived_(bytesReceived)
{
    // TODO: Handle connection dies before constructing the object
    if (!connPtr->connected())
        return;
    // Send HTTP/2 magic string
    connPtr->send(h2_preamble.data(), h2_preamble.length());
    *bytesSent_ += h2_preamble.length();

    // RFC 9113 3.4
    // > This sequence MUST be followed by a SETTINGS frame.
    SettingsFrame settingsFrame;
    settingsFrame.settings.emplace_back((uint16_t)H2SettingsKey::EnablePush,
                                        0);  // Disable push
    sendFrame(settingsFrame, 0);
}

void Http2Transport::handleFrameForStream(const internal::H2Frame &frame,
                                          int32_t streamId,
                                          uint8_t flags)
{
    auto it = streams.find(streamId);
    if (it == streams.end())
    {
        LOG_TRACE << "Non-existent stream id: " << streamId;
        killConnection(streamId,
                       StreamCloseErrorCode::ProtocolError,
                       "Non-existent stream id" + std::to_string(streamId));
        return;
    }
    auto &stream = it->second;

    if (std::holds_alternative<HeadersFrame>(frame))
    {
        if (stream.state != StreamState::ExpectingHeaders)
        {
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "Unexpected headers frame");
            return;
        }
        bool endStream = (flags & (uint8_t)H2HeadersFlags::EndStream);
        bool endHeaders = (flags & (uint8_t)H2HeadersFlags::EndHeaders);
        if (endStream && !endHeaders)
        {
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "This client does not support trailers");
            return;
        }
        if (!endHeaders)
        {
            auto &f = std::get<HeadersFrame>(frame);
            headerBufferRx.append((char *)f.headerBlockFragment.data(),
                                  f.headerBlockFragment.size());
            expectngContinuationStreamId = streamId;
            stream.state = StreamState::ExpectingContinuation;
            return;
        }
        auto &f = std::get<HeadersFrame>(frame);
        // This function handles error itself
        if (!parseAndApplyHeaders(stream,
                                  f.headerBlockFragment.data(),
                                  f.headerBlockFragment.size()))
            return;

        // There is no body in the response.
        if (endStream)
        {
            stream.state = StreamState::Finished;
            responseSuccess(stream);
            return;
        }
        stream.state = StreamState::ExpectingData;
    }
    else if (std::holds_alternative<ContinuationFrame>(frame))
    {
        auto &f = std::get<ContinuationFrame>(frame);
        if (stream.state != StreamState::ExpectingContinuation)
        {
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "Unexpected continuation frame");
            return;
        }

        headerBufferRx.append((char *)f.headerBlockFragment.data(),
                              f.headerBlockFragment.size());
        bool endHeaders = (flags & (uint8_t)H2HeadersFlags::EndHeaders) != 0;
        if (endHeaders)
        {
            stream.state = StreamState::ExpectingData;
            expectngContinuationStreamId = 0;
            bool ok = parseAndApplyHeaders(stream,
                                           headerBufferRx.peek(),
                                           headerBufferRx.readableBytes());
            headerBufferRx.retrieveAll();
            if (!ok)
                LOG_TRACE << "Failed to parse headers in continuation frame";
            return;
        }
    }
    else if (std::holds_alternative<DataFrame>(frame))
    {
        auto &f = std::get<DataFrame>(frame);
        auto [data, size] = f.getData();
        if (avaliableRxWindow < size)
        {
            killConnection(streamId,
                           StreamCloseErrorCode::FlowControlError,
                           "Too much for connection-level flow control");
            return;
        }
        else if (stream.avaliableRxWindow < size)
        {
            killConnection(streamId,
                           StreamCloseErrorCode::FlowControlError,
                           "Too much for stream-level flow control");
            return;
        }

        if (stream.state != StreamState::ExpectingData)
        {
            // XXX: Maybe this could be a RST_STREAM instead?
            killConnection(streamId,
                           StreamCloseErrorCode::ProtocolError,
                           "Unexpected data frame");
            return;
        }
        avaliableRxWindow -= size;
        stream.avaliableRxWindow -= size;
        LOG_TRACE << "Data frame received: size=" << size;

        stream.body.append((char *)data, size);
        if ((flags & (uint8_t)H2DataFlags::EndStream) != 0)
        {
            stream.state = StreamState::Finished;
            // This is not compliant with RFC 7540 8.1.2.6 sating that
            // > A response that is defined to have no payload
            // > .. can have a non-zero content-length header field,
            // > even though no content is included in DATA frames.
            // Not like that's going to not cause any problem.
            if (stream.contentLength &&
                stream.body.size() != *stream.contentLength)
            {
                LOG_TRACE << "content-length mismatch";
                responseErrored(streamId, ReqResult::BadResponse);
                return;
            }
            stream.response->setBody(std::move(stream.body));
            responseSuccess(stream);
            return;
        }

        if (stream.avaliableRxWindow < windowIncreaseThreshold)
        {
            WindowUpdateFrame windowUpdateFrame(windowIncreaseSize);
            sendFrame(windowUpdateFrame, streamId);
            stream.avaliableRxWindow += windowIncreaseSize;
        }
    }
    else if (std::holds_alternative<WindowUpdateFrame>(frame))
    {
        auto &f = std::get<WindowUpdateFrame>(frame);
        stream.avaliableTxWindow += f.windowSizeIncrement;
        if (avaliableTxWindow == 0)
            return;

        auto it = pendingDataSend.find(streamId);
        if (it == pendingDataSend.end())
            return;

        auto [sentOffset, done] = sendBodyForStream(stream, it->second);
        if (done)
            pendingDataSend.erase(it);
        else
            it->second = sentOffset;
    }
    else if (std::holds_alternative<RstStreamFrame>(frame))
    {
        auto &f = std::get<RstStreamFrame>(frame);
        LOG_TRACE << "RST_STREAM frame received: errorCode=" << f.errorCode;
        responseErrored(streamId, ReqResult::BadResponse);
        stream.state = StreamState::Finished;
    }
    else
    {
        LOG_TRACE << "Unsupported frame type for stream: " << streamId;
    }
}

internal::H2Stream &Http2Transport::createStream(int32_t streamId)
{
    auto it = streams.find(streamId);
    if (it != streams.end())
    {
        LOG_FATAL << "Stream id already in use! This should not happen";
        abort();
    }
    auto &stream = streams[streamId];
    stream.streamId = streamId;
    stream.avaliableTxWindow = initialTxWindowSize;
    stream.avaliableRxWindow = initialRxWindowSize;
    return stream;
}

void Http2Transport::responseSuccess(internal::H2Stream &stream)
{
    assert(stream.request != nullptr);
    assert(stream.callback);
    auto it = streams.find(stream.streamId);
    assert(it != streams.end());
    auto streamId = stream.streamId;
    // It is technically possible for a server to response before we fully
    // send the body. So cehck and remove.
    pendingDataSend.erase(streamId);
    // XXX: This callback seems to be able to cause the destruction of this
    // object
    respCallback(stream.response,
                 {std::move(stream.request), std::move(stream.callback)},
                 connPtr);
    streams.erase(it);  // NOTE: stream is now invalid

    if (bufferedRequests.empty())
        return;
    assert(streams.size() < maxConcurrentStreams);
    auto &[req, cb] = bufferedRequests.front();
    sendRequestInLoop(req, std::move(cb));
    bufferedRequests.pop();
}

void Http2Transport::responseErrored(int32_t streamId, ReqResult result)
{
    pendingDataSend.erase(streamId);

    auto it = streams.find(streamId);
    assert(it != streams.end());
    it->second.callback(result, nullptr);
    streams.erase(it);

    if (bufferedRequests.empty())
        return;
    auto &[req, cb] = bufferedRequests.front();
    sendRequestInLoop(req, std::move(cb));
    bufferedRequests.pop();
}

void Http2Transport::onError(ReqResult result)
{
    connPtr->getLoop()->assertInLoopThread();
    for (auto &[streamId, stream] : streams)
        stream.callback(result, nullptr);
    streams.clear();

    while (!bufferedRequests.empty())
    {
        auto &[req, cb] = bufferedRequests.front();
        cb(result, nullptr);
        bufferedRequests.pop();
    }

    pendingDataSend.clear();
}

void Http2Transport::killConnection(int32_t lastStreamId,
                                    StreamCloseErrorCode errorCode,
                                    std::string errorMsg)
{
    LOG_TRACE << "Killing connection with error: " << errorMsg;
    connPtr->getLoop()->assertInLoopThread();
    sendFrame(GoAwayFrame(lastStreamId, (uint32_t)errorCode, errorMsg), 0);

    for (auto &[streamId, stream] : streams)
        stream.callback(ReqResult::BadResponse, nullptr);
    errorCallback(ReqResult::BadResponse);
}

bool Http2Transport::handleConnectionClose()
{
    if (streams.size() == 0)
        return false;
    for (auto &[streamId, stream] : streams)
        stream.callback(ReqResult::BadResponse, nullptr);
    streams.clear();
    pendingDataSend.clear();
    return true;
}

std::pair<size_t, bool> Http2Transport::sendBodyForStream(
    internal::H2Stream &stream,
    const void *data,
    size_t size)
{
    auto streamId = stream.streamId;
    if (stream.avaliableTxWindow == 0 || avaliableTxWindow == 0)
        return {0, false};

    size_t maxSendSize = size;
    maxSendSize = (std::min)(maxSendSize, stream.avaliableTxWindow);
    maxSendSize = (std::min)(maxSendSize, avaliableTxWindow);
    bool sendEverything = maxSendSize == size;

    size_t i = 0;
    for (i = 0; i < size; i += maxFrameSize)
    {
        size_t remaining = size - i;
        size_t readSize = (std::min)(maxFrameSize, remaining);
        bool endStream = sendEverything && i + maxFrameSize >= size;
        const std::string_view sendData((const char *)data + i, readSize);
        DataFrame dataFrame(sendData, endStream);
        LOG_TRACE << "Sending data frame: size=" << readSize
                  << " endStream=" << dataFrame.endStream;
        sendFrame(dataFrame, streamId);

        stream.avaliableTxWindow -= readSize;
        avaliableTxWindow -= readSize;
    }
    i = (std::min)(i, size);
    return {i, sendEverything};
}

std::pair<size_t, bool> Http2Transport::sendBodyForStream(
    internal::H2Stream &stream,
    size_t offset)
{
    auto streamId = stream.streamId;
    if (stream.avaliableTxWindow == 0 || avaliableTxWindow == 0)
        return {offset, false};

    // Special handling for multipart because different underlying code
    auto mPtr = dynamic_cast<HttpFileUploadRequest *>(stream.request.get());
    if (mPtr)
    {
        // TODO: Don't put everything in memory. This causes a lot of memory
        // when uploading large files. Howerver, we are not doing better in 1.x
        // client either. So, meh.
        if (stream.multipartData.readableBytes() == 0)
            mPtr->renderMultipartFormData(stream.multipartData);
        auto &content = stream.multipartData;
        // CANNOT be empty. At least we are sending the boundary
        assert(content.readableBytes() > 0);
        auto [amount, done] =
            sendBodyForStream(stream, content.peek(), content.readableBytes());

        if (done)
            // force release memory
            stream.multipartData = trantor::MsgBuffer();
        return {offset + amount, done};
    }

    auto [amount, done] =
        sendBodyForStream(stream,
                          stream.request->body().data() + offset,
                          stream.request->body().length() - offset);
    assert(offset + amount <= stream.request->body().length());
    return {offset + amount, done};
}

void Http2Transport::sendFrame(const internal::H2Frame &frame, int32_t streamId)
{
    size_t oldSize = batchedSendBuffer.size();
    serializeFrame(batchedSendBuffer, frame, streamId);
    *bytesSent_ += batchedSendBuffer.size() - oldSize;
}

void Http2Transport::sendBufferedData()
{
    if (batchedSendBuffer.size() == 0)
        return;
    OByteStream buffer;
    std::swap(buffer, batchedSendBuffer);
    connPtr->send(std::move(buffer.buffer));
}
