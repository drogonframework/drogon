#include "Http2Transport.h"

#include <variant>

using namespace drogon;

static const std::string_view h2_preamble = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

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

    trantor::MsgBuffer buffer;
};

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

struct SettingsFrame
{
    std::vector<std::pair<uint16_t, uint32_t>> settings;
};

struct WindowUpdateFrame
{
    uint32_t windowSizeIncrement;
};

struct HeadersFrame
{
    uint8_t padLength = 0;
    bool exclusive = false;
    uint32_t streamDependency = 0;
    uint8_t weight = 0;
    std::vector<uint8_t> headerBlockFragment;
};

struct GoAwayFrame
{
    uint32_t lastStreamId;
    uint32_t errorCode;
    std::vector<uint8_t> additionalDebugData;
};

struct DataFrame
{
    uint8_t padLength = 0;
    bool endStream = false;
    std::vector<uint8_t> data;
};

using H2Frame =
    std::variant<SettingsFrame, WindowUpdateFrame, HeadersFrame, GoAwayFrame>;

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

static std::optional<SettingsFrame> parseSettingsFrame(ByteStream &payload)
{
    if (payload.size() % 6 != 0)
    {
        LOG_ERROR << "Invalid settings frame length";
        return std::nullopt;
    }

    SettingsFrame frame;
    LOG_TRACE << "Settings frame:";
    for (size_t i = 0; i < payload.size(); i += 6)
    {
        uint16_t key = payload.readU16BE();
        uint32_t value = payload.readU32BE();
        frame.settings.emplace_back(key, value);

        LOG_TRACE << "  key=" << key << " value=" << value;
    }
    return frame;
}

static std::optional<WindowUpdateFrame> parseWindowUpdateFrame(
    ByteStream &payload)
{
    if (payload.size() != 4)
    {
        LOG_ERROR << "Invalid window update frame length";
        return std::nullopt;
    }

    WindowUpdateFrame frame;
    // MSB is reserved for future use
    frame.windowSizeIncrement = payload.readU32BE() & 0x7fffffff;
    LOG_TRACE << "Window update frame: windowSizeIncrement="
              << frame.windowSizeIncrement;
    return frame;
}

static std::optional<GoAwayFrame> parseGoAwayFrame(ByteStream &payload)
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

    LOG_TRACE << "Go away frame: lastStreamId=" << frame.lastStreamId
              << " errorCode=" << frame.errorCode
              << " additionalDebugData=" << frame.additionalDebugData.size();
    return frame;
}

static std::optional<HeadersFrame> parseHeadersFrame(ByteStream &payload,
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
        LOG_TRACE << "Headers frame: padLength=" << frame.padLength;
    }
    if (priority)
    {
        auto [exclusive, streamDependency] = payload.readBI32BE();
        frame.exclusive = exclusive;
        frame.streamDependency = streamDependency;
        frame.weight = payload.readU8();
        LOG_TRACE << "Headers frame: exclusive=" << exclusive
                  << " streamDependency=" << streamDependency
                  << " weight=" << frame.weight;
    }
    if (endHeaders)
    {
        LOG_TRACE << "Headers frame: endHeaders";
    }
    if (endStream)
    {
        LOG_TRACE << "Headers frame: endStream";
    }

    frame.headerBlockFragment.resize(payload.remaining());
    payload.read(frame.headerBlockFragment.data(),
                 frame.headerBlockFragment.size());
    return frame;
}

static std::vector<uint8_t> serializeHeadsFrame(const HeadersFrame &frame)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(6 + frame.headerBlockFragment.size() + frame.padLength);
    // buffer.push_back(frame.padLength);
    // uint32_t streamDependency = frame.streamDependency;
    // if (frame.exclusive)
    //     streamDependency |= 1U << 31;
    // buffer.push_back(streamDependency >> 24);
    // buffer.push_back(streamDependency >> 16);
    // buffer.push_back(streamDependency >> 8);
    // buffer.push_back(streamDependency);
    // buffer.push_back(frame.weight);
    std::copy(frame.headerBlockFragment.begin(),
              frame.headerBlockFragment.end(),
              std::back_inserter(buffer));
    for (size_t i = 0; i < frame.padLength; ++i)
        buffer.push_back(0x0);
    return buffer;
}

static std::vector<uint8_t> serializeSettingsFrame(const SettingsFrame &frame)
{
    if (frame.settings.size() == 0)
        return std::vector<uint8_t>();
    std::vector<uint8_t> buffer;
    buffer.reserve(6 * frame.settings.size());
    for (auto &[key, value] : frame.settings)
    {
        buffer.push_back(key >> 8);
        buffer.push_back(key);
        buffer.push_back(value >> 24);
        buffer.push_back(value >> 16);
        buffer.push_back(value >> 8);
        buffer.push_back(value);
    }
    return buffer;
}

static std::vector<uint8_t> serializeWindowUpdateFrame(
    const WindowUpdateFrame &frame)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(4);
    buffer.push_back(frame.windowSizeIncrement >> 24);
    buffer.push_back(frame.windowSizeIncrement >> 16);
    buffer.push_back(frame.windowSizeIncrement >> 8);
    buffer.push_back(frame.windowSizeIncrement);
    return buffer;
}

static std::vector<uint8_t> serializeFrame(const H2Frame &frame,
                                           size_t streamId,
                                           uint8_t flags = 0)
{
    std::vector<uint8_t> buffer;
    uint8_t type;
    if (std::holds_alternative<HeadersFrame>(frame))
    {
        const auto &f = std::get<HeadersFrame>(frame);
        buffer = serializeHeadsFrame(f);
        type = (uint8_t)H2FrameType::Headers;
    }
    else if (std::holds_alternative<SettingsFrame>(frame))
    {
        const auto &f = std::get<SettingsFrame>(frame);
        buffer = serializeSettingsFrame(f);
        type = (uint8_t)H2FrameType::Settings;
    }
    else if (std::holds_alternative<WindowUpdateFrame>(frame))
    {
        const auto &f = std::get<WindowUpdateFrame>(frame);
        buffer = serializeWindowUpdateFrame(f);
        type = (uint8_t)H2FrameType::WindowUpdate;
    }
    else
    {
        LOG_ERROR << "Unsupported frame type";
        abort();
    }

    std::vector<uint8_t> full_frame;
    full_frame.reserve(9 + buffer.size());
    size_t length = buffer.size();
    assert(length <= 0xffffff);
    full_frame.push_back(length >> 16);
    full_frame.push_back(length >> 8);
    full_frame.push_back(length);
    full_frame.push_back(type);
    full_frame.push_back(flags);
    full_frame.push_back(streamId >> 24);
    full_frame.push_back(streamId >> 16);
    full_frame.push_back(streamId >> 8);
    full_frame.push_back(streamId);
    full_frame.insert(full_frame.end(), buffer.begin(), buffer.end());
    return full_frame;
}

// return streamId, frame, error and should continue parsing
// Note that error can orrcur on a stream level or the entire connection
// We need to handle both cases. Also it could happen that the TCP stream
// just cuts off in the middle of a frame (or header). We need to handle that
// too.
static std::tuple<std::optional<H2Frame>, size_t, bool> parseH2Frame(
    trantor::MsgBuffer *msg)
{
    if (msg->readableBytes() < 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame header";
        return {std::nullopt, 0, false};
    }

    uint8_t *ptr = (uint8_t *)msg->peek();
    ByteStream header(ptr, 9);

    // 24 bits length
    const uint32_t length = header.readU24BE();
    if (msg->readableBytes() < length + 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame";
        return {std::nullopt, 0, false};
    }

    const uint8_t type = header.readU8();
    const uint8_t flags = header.readU8();
    // MSB is reserved for future use
    const uint32_t streamId = header.readU32BE() & ((1U << 31) - 1);

    if (type >= (uint8_t)H2FrameType::NumEntries)
    {
        // TODO: Handle fatal protocol error
        LOG_ERROR << "Invalid H2 frame type: " << (int)type;
        return {std::nullopt, streamId, true};
    }

    LOG_TRACE << "H2 frame: length=" << length << " type=" << (int)type
              << " flags=" << (int)flags << " streamId=" << streamId;

    ByteStream payload(ptr + 9, length);
    std::optional<H2Frame> frame;
    if (type == (uint8_t)H2FrameType::Settings)
        frame = parseSettingsFrame(payload);
    else if (type == (uint8_t)H2FrameType::WindowUpdate)
        frame = parseWindowUpdateFrame(payload);
    else if (type == (uint8_t)H2FrameType::GoAway)
        frame = parseGoAwayFrame(payload);
    else if (type == (uint8_t)H2FrameType::Headers)
        frame = parseHeadersFrame(payload, flags);
    else
    {
        LOG_WARN << "Unsupported H2 frame type: " << (int)type;
        msg->retrieve(length + 9);
        return {std::nullopt, streamId, false};
    }

    if (payload.remaining() != 0)
        LOG_WARN << "Invalid H2 frame payload length or bug in parsing!!";

    msg->retrieve(length + 9);
    if (!frame)
    {
        LOG_ERROR << "Failed to parse H2 frame";
        return {std::nullopt, streamId, true};
    }

    return {frame, streamId, false};
}

void Http2Transport::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback)
{
    if (!serverSettingsReceived)
    {
        bufferedRequests.emplace_back(req, std::move(callback));
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
    frame.headerBlockFragment.resize(n);
    LOG_TRACE << "Sending headers frame";
    auto f = serializeFrame(frame, 1, 0x5);
    dump_hex_beautiful(f.data(), f.size());
    connPtr->send(f.data(), f.size());
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
        auto [frameOpt, streamId, error] = parseH2Frame(msg);

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

        // TODO: Figure out how to dispatch the frame to the right stream
        if (std::holds_alternative<WindowUpdateFrame>(frame))
        {
            auto &f = std::get<WindowUpdateFrame>(frame);
            if (streamId == 0)
            {
                avaliableWindowSize += f.windowSizeIncrement;
            }
        }
        else if (std::holds_alternative<SettingsFrame>(frame))
        {
            auto &f = std::get<SettingsFrame>(frame);
            for (auto &[key, value] : f.settings)
            {
                if (key == (uint16_t)H2SettingsKey::MaxConcurrentStreams)
                {
                    if (streamId == 0)
                        maxConcurrentStreams = value;
                    else
                        LOG_TRACE << "Ignoring max concurrent streams due to "
                                     "streamId != 0";
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

            LOG_TRACE << "Acknowledge settings frame";
            SettingsFrame ackFrame;
            auto b = serializeFrame(ackFrame, 0, 0x1);
            connPtr->send((const char *)b.data(), b.size());

            if (streamId == 0 && !serverSettingsReceived)
            {
                LOG_TRACE << "Server settings received. Sending our own "
                             "settings and WindowUpdate";
                SettingsFrame settingsFrame;
                settingsFrame.settings.emplace_back(
                    (uint16_t)H2SettingsKey::EnablePush, 0);  // Disable push
                auto b = serializeFrame(settingsFrame, 0);
                connPtr->send((const char *)b.data(), b.size());

                WindowUpdateFrame windowUpdateFrame;
                // TODO: Keep track and update the window size
                windowUpdateFrame.windowSizeIncrement = 200 * 1024 * 1024;
                auto b2 = serializeFrame(windowUpdateFrame, 0);
                connPtr->send((const char *)b2.data(), b2.size());

                serverSettingsReceived = true;
                for (auto &[req, cb] : bufferedRequests)
                    sendRequestInLoop(req, std::move(cb));
                bufferedRequests.clear();
            }
        }
        else if (std::holds_alternative<HeadersFrame>(frame))
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
                abort();
                return;
            }
            for (auto &[key, value] : headers)
                LOG_TRACE << "  " << key << ": " << value;
        }
        else if (std::holds_alternative<GoAwayFrame>(frame))
        {
            LOG_ERROR << "Go away frame received. Die!";
            auto &f = std::get<GoAwayFrame>(frame);
            // TODO: Depening on the streamId, we need to kill the entire
            // connection or just the stream
            errorCallback(ReqResult::BadResponse);
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
