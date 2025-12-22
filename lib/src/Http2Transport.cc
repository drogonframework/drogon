#include <optional>
#define NOMINMAX
#include "Http2Transport.h"
#include "HttpFileUploadRequest.h"
#include "drogon/utils/Utilities.h"
#include "hpack.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>

using namespace drogon;
using namespace drogon::internal;

static const std::string_view h2_preamble = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
static std::array<uint8_t, 8> pingOpaqueData = {0, 1, 2, 3, 4, 5, 6, 7};

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

template <typename Enum>
constexpr size_t enumMaxValue()
{
    using Underlying = std::underlying_type_t<Enum>;
    return (std::numeric_limits<Underlying>::max)();
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
    auto [_, windowSizeIncrement] = payload.readBI31BE();
    frame.windowSizeIncrement = windowSizeIncrement;
    if (frame.windowSizeIncrement == 0)
    {
        LOG_TRACE << "Flow control error: window size increment cannot be 0";
        return std::nullopt;
    }
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
        auto [exclusive, streamDependency] = payload.readBI31BE();
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
        uint32_t streamDependency = this->streamDependency | (1U << 31);
        stream.writeU32BE(streamDependency);
        stream.writeU8(weight);
    }

    if (endHeaders)
        flags |= (uint8_t)H2HeadersFlags::EndHeaders;
    if (endStream)
        flags |= (uint8_t)H2HeadersFlags::EndStream;
    stream.write(headerBlockFragment.data(), headerBlockFragment.size());
    if (padLength > 0)
    {
        stream.pad(padLength);
    }
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
    frame.lastStreamId = payload.readU32BE() & ~(1U << 31);
    frame.errorCode = payload.readU32BE();
    frame.additionalDebugData.resize(payload.remaining());
    payload.read(frame.additionalDebugData.data(), payload.remaining());
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
    if (payload.remaining() < frame.padLength)
    {
        LOG_TRACE << "data padding is larger than the payload size";
        return std::nullopt;
    }

    const size_t payloadSize = payload.remaining() - frame.padLength;
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
    if (padLength > 0)
    {
        flags |= (uint8_t)H2DataFlags::Padded;
        stream.writeU8(padLength);
    }
    auto [ptr, size] = getData();
    stream.write(ptr, size);

    if (padLength > 0)
    {
        stream.pad(padLength);
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

    auto [_, promisedStreamId] = payload.readBI31BE();
    frame.promisedStreamId = promisedStreamId;
    assert(payload.remaining() >= frame.padLength);
    size_t payloadSize = payload.remaining() - frame.padLength;
    if (payloadSize != 0)
    {
        frame.headerBlockFragment.resize(payloadSize);
        payload.read(frame.headerBlockFragment.data(),
                     frame.headerBlockFragment.size());
    }
    payload.skip(frame.padLength);
    return frame;
}

bool PushPromiseFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    if (endHeaders)
        flags |= (uint8_t)H2HeadersFlags::EndHeaders;
    if (padLength > 0)
    {
        flags |= (uint8_t)H2HeadersFlags::Padded;
        stream.writeU8(padLength);
    }
    assert(promisedStreamId > 0);
    stream.writeU32BE(promisedStreamId);
    stream.write(headerBlockFragment.data(), headerBlockFragment.size());
    if (padLength > 0)
    {
        stream.pad(padLength);
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
// return: {optional<H2Frame>, streamId, h2flags, isFatalError, consumedBytes}
static std::tuple<std::optional<H2Frame>, uint32_t, uint8_t, bool, uint32_t>
parseH2Frame(trantor::MsgBuffer *msg)
{
    if (msg->readableBytes() < 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame header";
        return {std::nullopt, 0, 0, false, 0};
    }

    uint8_t *ptr = (uint8_t *)msg->peek();
    ByteStream header(ptr, 9);

    // 24 bits length
    const uint32_t length = header.readU24BE();
    if (msg->readableBytes() < length + 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame";
        return {std::nullopt, 0, 0, false, 0};
    }

    const uint8_t type = header.readU8();
    const uint8_t flags = header.readU8();
    // MSB is reserved for future use
    const uint32_t streamId = header.readU32BE() & ((1U << 31) - 1);

    // This check is not need as HTTP/2 allows unknown frame types to be
    // ignored for forward compatibility.
    // if (type >= (uint8_t)H2FrameType::NumEntries)
    // {
    //     LOG_TRACE << "Unsupported H2 frame type: " << (int)type;
    //     return {std::nullopt, streamId, 0, true, 0};
    // }

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
        LOG_TRACE << "Unsupported H2 frame type: " << (int)type;
        msg->retrieve(length + 9);
        return {std::nullopt, streamId, 0, false, 0};
    }

    if (payload.remaining() != 0)
        LOG_WARN << "Invalid H2 frame payload length or bug in parsing!!";

    msg->retrieve(length + 9);
    if (!frame)
    {
        LOG_TRACE << "Failed to parse H2 frame of type: " << (int)type;
        return {std::nullopt, streamId, 0, true, 0};
    }

    return {frame, streamId, flags, false, length + 9};
}

void Http2Transport::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback)
{
    connPtr->getLoop()->assertInLoopThread();
    Defer d([this]() { sendBufferedData(); });
    if (streams.size() + 1 > maxConcurrentStreams)
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
        connectionErrored(streamId,
                          StreamCloseErrorCode::InternalError,
                          "Internal stream id conflict");
        return;
    }

    const auto &headers = req->headers();

    LOG_TRACE << "Sending HTTP/2 headers: size=" << headers.size();
    std::vector<KeyValPair> vec;
    vec.reserve(headers.size() + 5);
    vec.emplace_back(":method", req->methodString());
    const auto &params = req->parameters();
    if (params.empty())
        vec.emplace_back(":path", req->path().empty() ? "/" : req->path());
    else
    {
        std::string path = (req->path().empty() ? "/" : req->path()) + "?";
        for (auto const &[key, value] : params)
        {
            path += utils::urlEncodeComponent(key) + "=" +
                    utils::urlEncodeComponent(value) + "&";
        }
        path.pop_back();
        vec.emplace_back(":path", path);
    }
    std::string scheme = connPtr->isSSLConnection() ? "https" : "http";
    if (static_cast<drogon::HttpRequestImpl *>(req.get())->passThrough())
        scheme = (req->peerCertificate() != nullptr) ? "https" : "http";
    vec.emplace_back(":scheme", scheme);
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
    std::vector<uint8_t> encoded;
    encoded.reserve(maxCompressiedHeaderSize);
    // TODO: We should be able to avoid this allocation
    // NOTE: 2MB of encoded header size should be enough for everyone
    auto wbuf = hpackTx.MakeHpWBuffer(encoded, 2 * 1024 * 1024);
    if (additionalHeaderData_.size() > 0)
    {
        for (auto byte : additionalHeaderData_)
        {
            wbuf->Append(byte);
        }
        additionalHeaderData_.clear();
    }
    auto status = hpackTx.Encoder(*wbuf, vec);
    if (status != Success)
    {
        LOG_TRACE << "Failed to encode headers. Internal error or header "
                     "block too large";
        connectionErrored(streamId,
                          StreamCloseErrorCode::InternalError,
                          "Internal error or header block too large");
        return;
    }
    LOG_TRACE << "Encoded headers size: " << encoded.size();

    bool haveBody =
        req->body().length() > 0 ||
        (req->contentType() == CT_MULTIPART_FORM_DATA &&
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
        auto [it, inserted] = pendingDataSend.try_emplace(streamId, sentOffset);
        if (!inserted)
        {
            LOG_FATAL << "Stream id already in use! This should not happen";
            abort();
        }
    }
}

void Http2Transport::onRecvMessage(const trantor::TcpConnectionPtr &,
                                   trantor::MsgBuffer *msg)
{
    Defer d([this]() { sendBufferedData(); });
    LOG_TRACE << "HTTP/2 message received:";
    assert(bytesReceived_ != nullptr);
    LOG_TRACE << dump_hex_beautiful(msg->peek(), msg->readableBytes());
    while (true)
    {
        if (availableRxWindow < windowIncreaseThreshold)
        {
            WindowUpdateFrame windowUpdateFrame(windowIncreaseSize);
            sendFrame(windowUpdateFrame, 0);
            availableRxWindow += windowIncreaseSize;
        }

        if (msg->readableBytes() == 0)
            break;

        auto [frameOpt, streamId, flags, error, consumedBytes] =
            parseH2Frame(msg);
        *bytesReceived_ += consumedBytes;
        if (error)
        {
            LOG_TRACE << "Fatal protocol error happened during stream parsing";
            connectionErrored(
                streamId,
                StreamCloseErrorCode::ProtocolError,
                "Fatal protocol error happened during stream parsing");
            return;
        }
        else if (frameOpt.has_value() == false)
            break;  // not enough data. Wait for more

        assert(frameOpt.has_value());
        auto &frame = *frameOpt;

        if (firstSettingsReceived == false)
        {
            firstSettingsReceived = true;
            if (std::holds_alternative<SettingsFrame>(frame) == false ||
                streamId != 0)
            {
                LOG_TRACE
                    << "First frame recv must be SETTINGS frame on stream 0";
                connectionErrored(
                    streamId,
                    StreamCloseErrorCode::ProtocolError,
                    "First frame recv must be SETTINGS frame on stream 0");
                return;
            }
        }

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

            // Cannot use range-based for loop because of possible erasure in
            // streamErrored()
            auto it = streams.begin();
            while (it != streams.end())
            {
                if (it->first > f.lastStreamId)
                {
                    // Collect ID or handle careful erasure
                    auto current = it++;  // Advance iterator first
                    streamErrored(current->first,
                                  std::nullopt,
                                  ReqResult::BadResponse);
                }
                else
                {
                    ++it;
                }
            }
            // TODO: Should be half-closed but trantor doesn't support it
            // Nothing I can do for now
            connPtr->shutdown();
            return;
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
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "Expecting CONTINUATION frame for stream " +
                                  std::to_string(expectngContinuationStreamId));
            break;
        }

        if (std::holds_alternative<PushPromiseFrame>(frame))
        {
            LOG_TRACE << "Push promise frame received. Not supported yet. "
                         "Connection will die";
            connectionErrored(streamId,
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
            if (std::numeric_limits<decltype(availableTxWindow)>::max() -
                    availableTxWindow <
                f.windowSizeIncrement)
            {
                LOG_TRACE << "Flow control error: TX window size overflow";
                connectionErrored(streamId,
                                  StreamCloseErrorCode::FlowControlError,
                                  "TX window size overflow");
                break;
            }
            availableTxWindow += f.windowSizeIncrement;

            // Find if we have a stream that can be resumed
            if (currentDataSend.has_value() == false && pendingDataSend.empty())
                continue;

            auto it = currentDataSend.value_or(pendingDataSend.begin());
            while (it != pendingDataSend.end())
            {
                auto streamIt = streams.find(it->first);
                assert(streamIt != streams.end());  // FATAL: must exist
                auto [sentOffset, done] =
                    sendBodyForStream(streamIt->second, it->second);
                if (done)
                {
                    it = pendingDataSend.erase(it);
                    if (it == pendingDataSend.end())
                    {
                        currentDataSend = std::nullopt;
                        break;
                    }
                    currentDataSend = it;
                    continue;
                }
                it->second = sentOffset;
                if (availableTxWindow == 0)
                    break;

                ++it;
                currentDataSend = it;
            }
        }
        else if (std::holds_alternative<SettingsFrame>(frame))
        {
            auto &f = std::get<SettingsFrame>(frame);
            for (auto &[key, value] : f.settings)
            {
                if (key == (uint16_t)H2SettingsKey::HeaderTableSize)
                {
                    if (value > 20 * 1024 * 1024)
                    {
                        LOG_TRACE << "HeaderTableSize too large";
                        connectionErrored(streamId,
                                          StreamCloseErrorCode::ProtocolError,
                                          "HeaderTableSize too large");
                        break;
                    }
                    // 64 is large enough
                    auto newBuf =
                        hpackTx.MakeHpWBuffer(additionalHeaderData_, 64);
                    HeaderFieldInfo headerFieldInfo;
                    hpackTx.EncoderDynamicTableSizeUpdate(*newBuf,
                                                          value,
                                                          headerFieldInfo);
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
                        connectionErrored(
                            streamId,
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
                        connectionErrored(
                            streamId,
                            StreamCloseErrorCode::FlowControlError,
                            "InitialWindowSize too large");
                        break;
                    }
                    int64_t diff =
                        (int64_t)value - (int64_t)initialTxWindowSize;
                    initialTxWindowSize = value;

                    // If first initial window update received, we need to
                    // update all streams' available window size (even if)
                    // it is going negative)
                    if (!firstInitalWindowUpdateReceived)
                    {
                        continue;
                    }
                    for (auto &[_, stream] : streams)
                    {
                        stream.availableTxWindow += diff;
                    }
                    firstInitalWindowUpdateReceived = true;
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
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "HEADERS frame on stream 0");
            break;
        }
        else if (std::holds_alternative<DataFrame>(frame))
        {
            LOG_FATAL << "Protocol error: DATA frame on stream 0";
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "DATA frame on stream 0");
            break;
        }
        else if (std::holds_alternative<ContinuationFrame>(frame))
        {
            LOG_FATAL << "Protocol error: CONTINUATION frame on stream 0";
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "CONTINUATION frame on stream 0");
            break;
        }
        else if (std::holds_alternative<RstStreamFrame>(frame))
        {
            LOG_FATAL << "Protocol error: RST_STREAM frame on stream 0";
            connectionErrored(streamId,
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
                                          size_t size,
                                          bool isTrailers)
{
    std::vector<KeyValPair> headers;
    headers.reserve(6);  // some small amount to reduce reallocation
    // TODO: We should be able to avoid this allocation
    auto rbuf = hpackRx.MakeHpRBuffer((const uint8_t *)data, size);
    const auto status = hpackRx.Decoder(*rbuf, headers);
    const auto streamId = stream.streamId;
    if (status != Success)
    {
        LOG_TRACE << "Failed to decode headers";
        connectionErrored(streamId,
                          StreamCloseErrorCode::CompressionError,
                          "Failed to decode headers");
        return false;
    }
    for (auto &[key, value] : headers)
        LOG_TRACE << "  " << key << ": " << value;
    assert(stream.response || !isTrailers);
    if (stream.response == nullptr)
    {
        stream.response = std::make_shared<HttpResponseImpl>();
        stream.response->setVersion(Version::kHttp2);
    }

    constexpr std::array<std::string_view, 14> bannedTrailerHeaders = {
        "content-length",
        "host",
        "cache-control",
        "expect",
        "max-forwards",
        "pragma",
        "range",
        "te",
        "authorization",
        "set-cookie",
        "content-encoding",
        "content-type",
        "content-range",
        "trailer"};
    for (const auto &[key, value] : headers)
    {
        bool ok = true;
        if (isTrailers)
        {
            if (std::find(bannedTrailerHeaders.begin(),
                          bannedTrailerHeaders.end(),
                          key) != bannedTrailerHeaders.end())
            {
                LOG_TRACE << "Banned trailer header: " << key;
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return false;
            }
            if (key.front() == ':')
            {
                LOG_TRACE << "Pseudo header in trailers: " << key;
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return false;
            }
        }
        // TODO: Filter more pseudo headers
        if (key == "content-length")
        {
            auto sz = stosz(value);
            if (!sz)
            {
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return false;
            }
            stream.contentLength = std::move(sz);
        }
        else if (key == ":status")
        {
            auto status = stosz(value);
            if (!status || *status > enumMaxValue<HttpStatusCode>())
            {
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return false;
            }
            stream.response->setStatusCode((HttpStatusCode)*status);
            continue;
        }

        // Anti request smuggling. We look for \r or \n in the header
        // name or value. If we find one, we abort the stream.
        if (key.find_first_of("\r\n: ") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos)
        {
            LOG_TRACE << "Invalid characters in header: " << key;
            streamErrored(streamId,
                          StreamCloseErrorCode::ProtocolError,
                          ReqResult::BadResponse);
            return false;
        }

        // RFC 7540 Section 8.1.2:  request or response containing uppercase
        // header field names MUST be treated as malformed
        for (auto c : key)
        {
            if (isupper(c))
            {
                LOG_TRACE << "Uppercase header field name: " << key;
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return false;
            }
        }

        stream.response->addHeader(key, value);
    }
    return true;
}

Http2Transport::Http2Transport(trantor::TcpConnectionPtr connPtr,
                               size_t *bytesSent,
                               size_t *bytesReceived,
                               double pingIntervalSec)
    : connPtr(connPtr),
      bytesSent_(bytesSent),
      bytesReceived_(bytesReceived),
      pingIntervalSec_(pingIntervalSec),
      hpackRx(&maxRxDynamicTableSize)
{
    // TODO: Handle connection dies before constructing the object
    if (!connPtr->connected())
        return;
    // Send HTTP/2 magic string
    batchedSendBuffer.write(h2_preamble);
    *bytesSent_ += h2_preamble.length();

    // RFC 9113 3.4
    // > This sequence MUST be followed by a SETTINGS frame.
    SettingsFrame settingsFrame;
    settingsFrame.settings.emplace_back((uint16_t)H2SettingsKey::EnablePush,
                                        0);  // Disable push
    // Increase initial window size to our desired size -- the values supplied
    // by RFC is too small for practical use
    settingsFrame.settings.emplace_back(
        (uint16_t)H2SettingsKey::InitialWindowSize, desiredInitialRxWindowSize);
    initialRxWindowSize = desiredInitialRxWindowSize;
    sendFrame(settingsFrame, 0);
    sendBufferedData();

    // Setup ping timer
    if (pingIntervalSec_ > 0)
    {
        pingTimerId_ = connPtr->getLoop()->runEvery(pingIntervalSec_, [this]() {
            LOG_DEBUG << "Sending HTTP/2 ping frame";
            PingFrame pingFrame(pingOpaqueData, false);
            sendFrame(pingFrame, 0);
            sendBufferedData();
        });
    }
}

Http2Transport::~Http2Transport()
{
    if (pingTimerId_ != 0)
    {
        connPtr->getLoop()->invalidateTimer(pingTimerId_);
        pingTimerId_ = 0;
    }
}

void Http2Transport::handleFrameForStream(const internal::H2Frame &frame,
                                          int32_t streamId,
                                          uint8_t flags)
{
    auto it = streams.find(streamId);
    if (it == streams.end())
    {
        LOG_TRACE << "Non-existent stream id: " << streamId;
        connectionErrored(streamId,
                          StreamCloseErrorCode::ProtocolError,
                          "Non-existent stream id" + std::to_string(streamId));
        return;
    }
    auto &stream = it->second;

    if (std::holds_alternative<HeadersFrame>(frame))
    {
        if (stream.state != StreamState::ExpectingHeaders &&
            stream.state != StreamState::ExpectingData)  // Trailers
        {
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "Unexpected headers frame");
            return;
        }
        bool isTrailers = (stream.state == StreamState::ExpectingData);
        bool endStream = (flags & (uint8_t)H2HeadersFlags::EndStream);
        bool endHeaders = (flags & (uint8_t)H2HeadersFlags::EndHeaders);
        if (!endHeaders)
        {
            auto &f = std::get<HeadersFrame>(frame);
            headerBufferRx.append((char *)f.headerBlockFragment.data(),
                                  f.headerBlockFragment.size());
            expectngContinuationStreamId = streamId;
            stream.state = isTrailers
                               ? StreamState::ExpectingContinuationTrailers
                               : StreamState::ExpectingContinuation;
            return;
        }
        auto &f = std::get<HeadersFrame>(frame);
        // This function handles error itself
        if (!parseAndApplyHeaders(stream,
                                  f.headerBlockFragment.data(),
                                  f.headerBlockFragment.size(),
                                  isTrailers))
            return;

        // There is no body in the response.
        if (endStream)
        {
            stream.state = StreamState::Finished;
            responseSuccess(stream);
            return;
        }

        if (isTrailers)
        {
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "Data after trailers");
        }
        stream.state = StreamState::ExpectingData;
    }
    else if (std::holds_alternative<ContinuationFrame>(frame))
    {
        auto &f = std::get<ContinuationFrame>(frame);
        if (stream.state != StreamState::ExpectingContinuation &&
            stream.state != StreamState::ExpectingContinuationTrailers)
        {
            connectionErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              "Unexpected continuation frame");
            return;
        }

        headerBufferRx.append((char *)f.headerBlockFragment.data(),
                              f.headerBlockFragment.size());
        bool isTrailers =
            (stream.state == StreamState::ExpectingContinuationTrailers);
        bool endHeaders = (flags & (uint8_t)H2HeadersFlags::EndHeaders) != 0;

        if (endHeaders && !isTrailers)
        {
            stream.state = StreamState::ExpectingData;
            expectngContinuationStreamId = 0;
            bool ok = parseAndApplyHeaders(stream,
                                           headerBufferRx.peek(),
                                           headerBufferRx.readableBytes(),
                                           isTrailers);
            headerBufferRx.retrieveAll();
            if (!ok)
                LOG_TRACE << "Failed to parse headers in continuation frame";
            return;
        }

        stream.state = StreamState::Finished;
        responseSuccess(stream);
        return;
    }
    else if (std::holds_alternative<DataFrame>(frame))
    {
        auto &f = std::get<DataFrame>(frame);
        auto [data, size] = f.getData();
        if (availableRxWindow < size)
        {
            connectionErrored(streamId,
                              StreamCloseErrorCode::FlowControlError,
                              "Too much for connection-level flow control");
            return;
        }
        else if (stream.availableRxWindow < size)
        {
            connectionErrored(streamId,
                              StreamCloseErrorCode::FlowControlError,
                              "Too much for stream-level flow control");
            return;
        }

        if (stream.state != StreamState::ExpectingData)
        {
            streamErrored(streamId,
                          StreamCloseErrorCode::ProtocolError,
                          ReqResult::BadResponse);
            return;
        }
        availableRxWindow -= size;
        stream.availableRxWindow -= size;
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
                streamErrored(streamId,
                              StreamCloseErrorCode::ProtocolError,
                              ReqResult::BadResponse);
                return;
            }
            responseSuccess(stream);
            return;
        }

        if (stream.availableRxWindow < windowIncreaseThreshold)
        {
            WindowUpdateFrame windowUpdateFrame(windowIncreaseSize);
            sendFrame(windowUpdateFrame, streamId);
            stream.availableRxWindow += windowIncreaseSize;
        }
    }
    else if (std::holds_alternative<WindowUpdateFrame>(frame))
    {
        auto &f = std::get<WindowUpdateFrame>(frame);
        if (std::numeric_limits<decltype(stream.availableTxWindow)>::max() -
                stream.availableTxWindow <
            f.windowSizeIncrement)
        {
            LOG_TRACE << "Flow control error: stream TX window size overflow";
            connectionErrored(streamId,
                              StreamCloseErrorCode::FlowControlError,
                              "Stream TX window size overflow");
            return;
        }
        stream.availableTxWindow += f.windowSizeIncrement;
        if (availableTxWindow == 0)
            return;

        auto it = pendingDataSend.find(streamId);
        if (it == pendingDataSend.end())
            return;

        auto [sentOffset, done] = sendBodyForStream(stream, it->second);
        if (done)
        {
            bool should_increment = currentDataSend.has_value() &&
                                    (*currentDataSend)->second == streamId;
            if (should_increment)
            {
                auto next = pendingDataSend.erase(it);
                if (next != pendingDataSend.end())
                    currentDataSend = next;
                else
                    currentDataSend = std::nullopt;
            }
        }
        else
            it->second = sentOffset;
    }
    else if (std::holds_alternative<RstStreamFrame>(frame))
    {
        // WTF? RST_STREAM after response is already sent? Ignore as we
        // already called the callback
        if (stream.state == StreamState::Finished)
            return;
        auto &f = std::get<RstStreamFrame>(frame);
        LOG_TRACE << "RST_STREAM frame received: errorCode=" << f.errorCode;
        stream.state = StreamState::Finished;
        streamErrored(streamId, std::nullopt, ReqResult::BadResponse);
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
    stream.availableTxWindow = initialTxWindowSize;
    stream.availableRxWindow = initialRxWindowSize;
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
    // send the body. So check and remove.
    pendingDataSend.erase(streamId);
    // XXX: This callback seems to be able to cause the destruction of this
    // object
    stream.response->setBody(std::move(stream.body));
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

void Http2Transport::streamErrored(
    int32_t streamId,
    std::optional<StreamCloseErrorCode> errorCode,
    ReqResult result)
{
    pendingDataSend.erase(streamId);

    auto it = streams.find(streamId);
    if (errorCode.has_value())
    {
        // Send RST_STREAM frame
        RstStreamFrame rstFrame(*errorCode);
        sendFrame(rstFrame, streamId);
    }
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

void Http2Transport::connectionErrored(int32_t lastStreamId,
                                       StreamCloseErrorCode errorCode,
                                       std::string errorMsg)
{
    (void)lastStreamId;
    LOG_ERROR << "Client killing HTTP/2 connection with error: " << errorMsg;
    connPtr->getLoop()->assertInLoopThread();

    // last stream ID = 0: we haven't processed any server-initiated streams (no
    // PUSH_PROMISE yet)
    sendFrame(GoAwayFrame(0, (uint32_t)errorCode, std::move(errorMsg)), 0);

    for (auto &[streamId, stream] : streams)
        stream.callback(ReqResult::BadResponse, nullptr);
    streams.clear();  // Add this to avoid issues
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
    if (stream.availableTxWindow == 0 || availableTxWindow == 0)
        return {0, false};

    int64_t maxSendSize = size;
    maxSendSize = (std::min)(maxSendSize, stream.availableTxWindow);
    maxSendSize = (std::min)(maxSendSize, availableTxWindow);
    bool sendEverything = maxSendSize == size;

    size_t sent = 0;
    for (size_t i = 0; i < maxSendSize; i += maxFrameSize)
    {
        size_t remaining = maxSendSize - i;
        size_t readSize = (std::min)(maxFrameSize, remaining);
        bool endStream = sendEverything && (i + readSize >= maxSendSize);
        const std::string_view sendData((const char *)data + i, readSize);
        DataFrame dataFrame(sendData, endStream);
        LOG_TRACE << "Sending data frame: size=" << readSize
                  << " endStream=" << dataFrame.endStream;
        sent += readSize;
        sendFrame(dataFrame, streamId);

        stream.availableTxWindow -= readSize;
        availableTxWindow -= readSize;
    }
    return {sent, sendEverything};
}

std::pair<size_t, bool> Http2Transport::sendBodyForStream(
    internal::H2Stream &stream,
    size_t offset)
{
    auto streamId = stream.streamId;
    if (stream.availableTxWindow == 0 || availableTxWindow == 0)
        return {offset, false};

    // Special handling for multipart because different underlying code
    auto mPtr = dynamic_cast<HttpFileUploadRequest *>(stream.request.get());
    if (mPtr)
    {
        // TODO: Don't put everything in memory. This causes a lot of memory
        // when uploading large files. However, we are not doing better in 1.x
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

    // proactively send the data if it's too large. Avoid potential reallocation
    // of the send buffer
    if (batchedSendBuffer.size() > 0x20000)
        sendBufferedData();
}

void Http2Transport::sendBufferedData()
{
    if (batchedSendBuffer.size() == 0)
        return;
    OByteStream buffer;
    std::swap(buffer, batchedSendBuffer);
    connPtr->send(std::move(buffer.buffer));
}
