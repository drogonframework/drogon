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

struct SettingsFrame
{
    std::vector<std::pair<uint16_t, uint32_t>> settings;
};

struct WindowUpdateFrame
{
    uint32_t windowSizeIncrement;
};

using H2Frame = std::variant<SettingsFrame, WindowUpdateFrame>;

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

// return streamId, frame, error and should continue parsing
// Note that error can orrcur on a stream level or the entire connection
// We need to handle both cases. Also it could happen that the TCP stream
// just cuts off in the middle of a frame (or header). We need to handle that
// too.
// std::tuple<std::optional<H2Frame>, size_t, bool>
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

void Http2Transport::onRecvMessage(const trantor::TcpConnectionPtr &,
                                   trantor::MsgBuffer *msg)
{
    LOG_TRACE << "HTTP/2 message received:";
    assert(bytesReceived_ != nullptr);
    *bytesReceived_ += msg->readableBytes();
    dump_hex_beautiful(msg->peek(), msg->readableBytes());
    while (true)
    {
        auto [frameOpt, streamId, error] = parseH2Frame(msg);

        if (error && streamId == 0)
        {
            abort();
        }
        else if (frameOpt.has_value() == false && !error)
            break;
        else if (!frameOpt.has_value())
        {
            LOG_ERROR << "Failed to parse H2 frame??? Shouldn't happen though";
            abort();
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
        }
        else
        {
            // TODO: Remove this once we support all frame types
            // in that case it'll be a parsing error or bad server
            LOG_ERROR << "Boom! The client does not understand this frame";
        }
    }

    throw std::runtime_error("HTTP/2 onRecvMessage not implemented");
}

Http2Transport::Http2Transport(trantor::TcpConnectionPtr connPtr,
                               size_t *bytesSent,
                               size_t *bytesReceived)
    : connPtr(connPtr), bytesSent_(bytesSent), bytesReceived_(bytesReceived)
{
    connPtr->send(h2_preamble.data(), h2_preamble.length());
}
