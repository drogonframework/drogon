#include "Http2Transport.h"

#include <variant>

using namespace drogon;
using namespace drogon::internal;

static const std::string_view h2_preamble = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

static std::vector<uint8_t> s2vec(const std::string &str)
{
    std::vector<uint8_t> vec(str.size());
    memcpy(vec.data(), str.data(), str.size());
    return vec;
}

static std::optional<size_t> stosz(const std::string &str)
{
    try
    {
        return std::stoull(str);
    }
    catch (const std::exception &e)
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

static GoAwayFrame goAway(int32_t sid,
                          const std::string &msg,
                          StreamCloseErrorCode ec)
{
    GoAwayFrame frame;
    frame.additionalDebugData = s2vec(msg);
    frame.errorCode = (uint32_t)ec;
    frame.lastStreamId = sid;
    return frame;
}

namespace drogon::internal
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
        assert(offset <= length - 3);
        uint32_t res =
            ptr[offset] << 16 | ptr[offset + 1] << 8 | ptr[offset + 2];
        offset += 3;
        return res;
    }

    uint32_t readU32BE()
    {
        assert(offset <= length - 4);
        uint32_t res = ptr[offset] << 24 | ptr[offset + 1] << 16 |
                       ptr[offset + 2] << 8 | ptr[offset + 3];
        offset += 4;
        return res;
    }

    std::pair<bool, int32_t> readBI32BE()
    {
        assert(offset <= length - 4);
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
        assert(offset <= length - 2);
        uint16_t res = ptr[offset] << 8 | ptr[offset + 1];
        offset += 2;
        return res;
    }

    uint8_t readU8()
    {
        assert(offset <= length - 1);
        return ptr[offset++];
    }

    void read(uint8_t *buffer, size_t size)
    {
        assert(offset <= length - size || size == 0);
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
        assert(offset <= length - n || n == 0);
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

    void write(const uint8_t *ptr, size_t size)
    {
        buffer.append((char *)ptr, size);
    }

    void overwriteU24BE(size_t offset, uint32_t value)
    {
        assert(value <= 0xffffff);
        assert(offset <= buffer.readableBytes() - 3);
        auto ptr = (uint8_t *)buffer.peek() + offset;
        ptr[0] = value >> 16;
        ptr[1] = value >> 8;
        ptr[2] = value;
    }

    void overwriteU8(size_t offset, uint8_t value)
    {
        assert(offset <= buffer.readableBytes() - 1);
        auto ptr = (uint8_t *)buffer.peek() + offset;
        ptr[0] = value;
    }

    uint8_t *peek()
    {
        return (uint8_t *)buffer.peek();
    }

    trantor::MsgBuffer buffer;
};

std::optional<SettingsFrame> SettingsFrame::parse(ByteStream &payload,
                                                  uint8_t flags)
{
    if (payload.size() % 6 != 0)
    {
        LOG_ERROR << "Invalid settings frame length";
        return std::nullopt;
    }

    SettingsFrame frame;
    if ((flags & 0x1) != 0)
    {
        frame.ack = true;
        if (payload.size() != 0)
        {
            LOG_ERROR << "Settings frame with ACK flag set should have "
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
        LOG_ERROR << "Invalid window update frame length";
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
        LOG_ERROR << "MSB of windowSizeIncrement should be 0";
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

    HeadersFrame frame;
    if (padded)
    {
        frame.padLength = payload.readU8();
    }
    if (priority)
    {
        auto [exclusive, streamDependency] = payload.readBI32BE();
        frame.exclusive = exclusive;
        frame.streamDependency = streamDependency;
        frame.weight = payload.readU8();
    }
    if (endHeaders)
    {
        frame.endHeaders = true;
    }
    if (endStream)
    {
        frame.endStream = true;
    }

    int64_t payloadSize = payload.remaining() - frame.padLength;
    if (payloadSize < 0)
    {
        LOG_ERROR << "headers padding is larger than the payload size";
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
        LOG_ERROR << "Invalid go away frame length";
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
    if (endStream)
    {
        frame.endStream = true;
    }

    int32_t payloadSize = payload.remaining() - frame.padLength;
    if (payloadSize < 0)
    {
        LOG_ERROR << "data padding is larger than the payload size";
        return std::nullopt;
    }

    frame.data.resize(payloadSize);
    payload.read(frame.data.data(), frame.data.size());
    payload.skip(frame.padLength);
    return frame;
}

bool DataFrame::serialize(OByteStream &stream, uint8_t &flags) const
{
    flags = 0x0;
    stream.write(data.data(), data.size());
    if (padLength > 0)
    {
        flags |= (uint8_t)H2DataFlags::Padded;
        for (size_t i = 0; i < padLength; ++i)
            stream.writeU8(0x0);
    }
    return true;
}
}  // namespace drogon::internal

// Print the HEX and ASCII representation of the buffer side by side
// 16 bytes per line. Same function as the xdd command in linux.
static void dump_hex_beautiful(const void *ptr, size_t size)
{
    for (size_t i = 0; i < size; i += 16)
    {
        printf("%08zx: ", i);
        for (size_t j = 0; j < 16; ++j)
        {
            if (i + j < size)
            {
                printf("%02x ", ((unsigned char *)ptr)[i + j]);
            }
            else
            {
                printf("   ");
            }
        }
        printf(" ");
        for (size_t j = 0; j < 16; ++j)
        {
            if (i + j < size)
            {
                if (((unsigned char *)ptr)[i + j] >= 32 &&
                    ((unsigned char *)ptr)[i + j] < 127)
                {
                    printf("%c", ((unsigned char *)ptr)[i + j]);
                }
                else
                {
                    printf(".");
                }
            }
        }
        printf("\n");
    }
}

static trantor::MsgBuffer serializeFrame(const H2Frame &frame, size_t streamId)
{
    OByteStream buffer;
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
    else
    {
        LOG_ERROR << "Unsupported frame type";
        abort();
    }

    if (!ok)
    {
        LOG_ERROR << "Failed to serialize frame";
        abort();
    }

    auto length = buffer.buffer.readableBytes() - 9;
    buffer.overwriteU24BE(0, length);
    buffer.overwriteU8(3, type);
    buffer.overwriteU8(4, flags);
    return buffer.buffer;
}

// return streamId, frame, error and should continue parsing
// Note that error can orrcur on a stream level or the entire connection
// We need to handle both cases. Also it could happen that the TCP stream
// just cuts off in the middle of a frame (or header). We need to handle that
// too.
static std::tuple<std::optional<H2Frame>, size_t, uint8_t, bool> parseH2Frame(
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
        LOG_ERROR << "Invalid H2 frame type: " << (int)type;
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
        LOG_ERROR << "Failed to parse H2 frame of type: " << (int)type;
        return {std::nullopt, streamId, 0, true};
    }

    return {frame, streamId, flags, false};
}

void Http2Transport::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback)
{
    connPtr->getLoop()->assertInLoopThread();
    if (!serverSettingsReceived)
    {
        bufferedRequests.push({req, std::move(callback)});
        return;
    }

    const int32_t streamId = nextStreamId();
    assert(streamId % 2 == 1);
    LOG_TRACE << "Sending HTTP/2 request: streamId=" << streamId;
    if (streams.find(streamId) != streams.end())
    {
        LOG_FATAL << "Stream id already in use! This should not happen";
        connPtr->send(
            serializeFrame(goAway(streamId,
                                  "replicated internal stream id",
                                  StreamCloseErrorCode::InternalError),
                           0));
        errorCallback(ReqResult::BadResponse);
        return;
    }

    auto headers = req->headers();
    HeadersFrame frame;
    frame.padLength = 0;
    frame.exclusive = false;
    frame.streamDependency = 0;
    frame.weight = 0;
    frame.headerBlockFragment.resize(1024);

    LOG_TRACE << "Sending HTTP/2 headers: size=" << headers.size();
    hpack::HPacker::KeyValueVector headersToEncode;
    const std::array<std::string_view, 2> headersToSkip = {
        {"host", "connection"}};
    headersToEncode.emplace_back(":method", req->methodString());
    headersToEncode.emplace_back(":path", req->path());
    headersToEncode.emplace_back(":scheme",
                                 connPtr->isSSLConnection() ? "https" : "http");
    headersToEncode.emplace_back(":authority", req->getHeader("host"));
    for (auto &[key, value] : headers)
    {
        if (std::find(headersToSkip.begin(), headersToSkip.end(), key) !=
            headersToSkip.end())
            continue;

        headersToEncode.emplace_back(key, value);
    }

    LOG_TRACE << "Final headers size: " << headersToEncode.size();
    for (auto &[key, value] : headersToEncode)
        LOG_TRACE << "  " << key << ": " << value;
    int n = hpackTx.encode(headersToEncode,
                           frame.headerBlockFragment.data(),
                           frame.headerBlockFragment.size());
    if (n < 0)
    {
        LOG_ERROR << "Failed to encode headers";
        abort();
        return;
    }
    // TODO: Send CONTINUATION frames if the header block fragment is too large
    if (n > 0x7fff)
    {
        LOG_ERROR << "Header block fragment too large";
        abort();
        return;
    }
    frame.headerBlockFragment.resize(n);
    frame.endHeaders = true;
    frame.endStream = true;
    LOG_TRACE << "Sending headers frame";
    auto f = serializeFrame(frame, streamId);
    dump_hex_beautiful(f.peek(), f.readableBytes());
    connPtr->send(f);

    auto &stream = createStream(streamId);
    stream.callback = std::move(callback);
    stream.request = req;
}

void Http2Transport::onRecvMessage(const trantor::TcpConnectionPtr &,
                                   trantor::MsgBuffer *msg)
{
    LOG_TRACE << "HTTP/2 message received:";
    assert(bytesReceived_ != nullptr);
    *bytesReceived_ += msg->readableBytes();
    dump_hex_beautiful(msg->peek(), msg->readableBytes());
    while (true)
    {
        // FIXME: The code cannot distinguish between a out-of-data and
        // unsupported frame type. We need to fix this as it should be handled
        // differently.
        auto [frameOpt, streamId, flags, error] = parseH2Frame(msg);

        if (error && streamId == 0)
        {
            LOG_ERROR << "Fatal protocol error happened on stream 0 (global)";
            errorCallback(ReqResult::BadResponse);
        }
        else if (frameOpt.has_value() == false && !error)
            break;
        else if (!frameOpt.has_value())
        {
            LOG_ERROR << "Failed to parse H2 frame??? Shouldn't happen though";
            errorCallback(ReqResult::BadResponse);
        }
        auto &frame = *frameOpt;

        if (streamId != 0)
        {
            handleFrameForStream(frame, streamId, flags);
            continue;
        }

        // This point forawrd, we are handling frames for stream 0
        if (std::holds_alternative<WindowUpdateFrame>(frame))
        {
            auto &f = std::get<WindowUpdateFrame>(frame);
            avaliableWindowSize += f.windowSizeIncrement;
        }
        else if (std::holds_alternative<SettingsFrame>(frame))
        {
            auto &f = std::get<SettingsFrame>(frame);
            for (auto &[key, value] : f.settings)
            {
                if (key == (uint16_t)H2SettingsKey::MaxConcurrentStreams)
                {
                    if (value == 0)
                    {
                        connPtr->send(serializeFrame(
                            goAway(streamId,
                                   "MaxConcurrentStreams cannot be 0",
                                   StreamCloseErrorCode::ProtocolError),
                            0));
                    }
                    maxConcurrentStreams = value;
                }
                else if (key == (uint16_t)H2SettingsKey::MaxFrameSize)
                {
                    maxFrameSize = value;
                }
                else
                {
                    LOG_TRACE << "Unsupported settings key: " << key;
                }
            }

            if (!serverSettingsReceived)
            {
                LOG_TRACE << "Server settings received. Sending our own "
                             "settings and WindowUpdate";
                SettingsFrame settingsFrame;
                settingsFrame.settings.emplace_back(
                    (uint16_t)H2SettingsKey::EnablePush, 0);  // Disable push
                connPtr->send(serializeFrame(settingsFrame, 0));

                WindowUpdateFrame windowUpdateFrame;
                // TODO: Keep track and update the window size
                windowUpdateFrame.windowSizeIncrement = 200 * 1024 * 1024;
                connPtr->send(serializeFrame(windowUpdateFrame, 0));

                serverSettingsReceived = true;
                while (!bufferedRequests.empty())
                {
                    auto &[req, cb] = bufferedRequests.front();
                    sendRequestInLoop(req, std::move(cb));
                    bufferedRequests.pop();
                }
            }

            // Somehow nghttp2 wants us to send ACK after sending our
            // preferences??
            if (flags == 1)
                continue;
            LOG_TRACE << "Acknowledge settings frame";
            SettingsFrame ackFrame;
            ackFrame.ack = true;
            connPtr->send(serializeFrame(ackFrame, streamId));
        }
        else if (std::holds_alternative<HeadersFrame>(frame))
        {
            // Should never show up on stream 0
            LOG_FATAL << "Protocol error: HEADERS frame on stream 0";
            errorCallback(ReqResult::BadResponse);
        }
        else if (std::holds_alternative<DataFrame>(frame))
        {
            LOG_FATAL << "Protocol error: DATA frame on stream 0";
            errorCallback(ReqResult::BadResponse);
        }
        else if (std::holds_alternative<GoAwayFrame>(frame))
        {
            auto &f = std::get<GoAwayFrame>(frame);
            if (f.errorCode != 0)
            {
                LOG_ERROR << "Go away frame on stream 0 received. Die!";
                errorCallback(ReqResult::BadResponse);
            }
            else
            {
                // We shouldn't have any requests in flight if the server is
                // sending us a go away frame to gracefully shutdown
                // But in case we do, we should treat them as network failures
                assert(streams.empty());
                errorCallback(ReqResult::NetworkFailure);
            }
            connPtr->shutdown();
        }
        else
        {
            // TODO: Remove this once we support all frame types
            // in that case it'll be a parsing error or bad server
            LOG_ERROR << "Boom! The client does not understand this frame";
        }
    }
}

Http2Transport::Http2Transport(trantor::TcpConnectionPtr connPtr,
                               size_t *bytesSent,
                               size_t *bytesReceived)
    : connPtr(connPtr), bytesSent_(bytesSent), bytesReceived_(bytesReceived)
{
    connPtr->send(h2_preamble.data(), h2_preamble.length());
}

void Http2Transport::handleFrameForStream(const internal::H2Frame &frame,
                                          int32_t streamId,
                                          uint8_t flags)
{
    auto it = streams.find(streamId);
    if (it == streams.end())
    {
        LOG_ERROR << "Non-existent stream id: " << streamId;
        connPtr->send(serializeFrame(
            goAway(streamId,
                   "Non-existent stream id " + std::to_string(streamId),
                   StreamCloseErrorCode::ProtocolError),
            0));
        return;
    }
    auto &stream = it->second;

    if (std::holds_alternative<HeadersFrame>(frame))
    {
        auto &f = std::get<HeadersFrame>(frame);
        LOG_TRACE << "Headers frame received: size="
                  << f.headerBlockFragment.size();
        hpack::HPacker::KeyValueVector headers;
        int n = hpackRx.decode(f.headerBlockFragment.data(),
                               f.headerBlockFragment.size(),
                               headers);
        if (n < 0)
        {
            LOG_ERROR << "Failed to decode headers";
            streamFinished(streamId,
                           ReqResult::BadResponse,
                           StreamCloseErrorCode::CompressionError,
                           "Failed to decode headers");
            return;
        }
        for (auto &[key, value] : headers)
            LOG_TRACE << "  " << key << ": " << value;
        it->second.response = std::make_shared<HttpResponseImpl>();
        for (const auto &[key, value] : headers)
        {
            // TODO: Filter more pseudo headers
            if (key == "content-length")
            {
                auto sz = stosz(value);
                if (!sz)
                {
                    streamFinished(streamId,
                                   ReqResult::BadResponse,
                                   StreamCloseErrorCode::ProtocolError,
                                   "Invalid content-length header");
                    return;
                }
                it->second.contentLength = std::move(sz);
            }
            if (key == ":status")
            {
                // TODO: Validate status code
                it->second.response->setStatusCode(
                    (drogon::HttpStatusCode)std::stoi(value));
                continue;
            }

            // TODO: Anti request smuggling via adding \r\n to the header
            it->second.response->addHeader(key, value);
        }

        // There is no body in the response.
        if ((flags & (uint8_t)H2HeadersFlags::EndStream))
        {
            streamFinished(stream);
            return;
        }
    }
    else if (std::holds_alternative<DataFrame>(frame))
    {
        auto &f = std::get<DataFrame>(frame);
        LOG_TRACE << "Data frame received: size=" << f.data.size();

        stream.body.append((char *)f.data.data(), f.data.size());
        if ((flags & (uint8_t)H2DataFlags::EndStream) != 0)
        {
            if (stream.contentLength &&
                stream.body.readableBytes() != *stream.contentLength)
            {
                LOG_ERROR << "Content-length mismatch";
                streamFinished(streamId,
                               ReqResult::BadResponse,
                               StreamCloseErrorCode::ProtocolError,
                               "Content-length mismatch");
                return;
            }
            // TODO: Optmize setting body
            std::string body(stream.body.peek(), stream.body.readableBytes());
            stream.response->setBody(std::move(body));
            streamFinished(stream);
            return;
        }
    }
    else
    {
        LOG_ERROR << "Unsupported frame type for stream: " << streamId;
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
    return stream;
}

void Http2Transport::streamFinished(internal::H2Stream &stream)
{
    assert(stream.request != nullptr);
    assert(stream.callback);
    auto it = streams.find(stream.streamId);
    assert(it != streams.end());
    respCallback(stream.response, {stream.request, stream.callback}, connPtr);
    streams.erase(it);
    retireStreamId(stream.streamId);
}

void Http2Transport::streamFinished(int32_t streamId,
                                    ReqResult result,
                                    StreamCloseErrorCode errorCode,
                                    std::string errorMsg)
{
    auto it = streams.find(streamId);
    assert(it != streams.end());

    connPtr->send(serializeFrame(goAway(streamId, errorMsg, errorCode), 0));

    it->second.callback(result, nullptr);
    streams.erase(it);
    retireStreamId(streamId);
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
}