#include "Http2Transport.h"

using namespace drogon;

static const std::string_view h2_preamble = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

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

static std::optional<SettingsFrame> parseSettingsFrame(const uint8_t *ptr,
                                                       size_t length)
{
    if (length % 6 != 0)
    {
        LOG_ERROR << "Invalid settings frame length";
        return std::nullopt;
    }

    SettingsFrame frame;
    LOG_TRACE << "Settings frame:";
    for (size_t i = 0; i < length; i += 6)
    {
        uint16_t key = ptr[i] << 8 | ptr[i + 1];
        uint32_t value =
            ptr[i + 2] << 24 | ptr[i + 3] << 16 | ptr[i + 4] << 8 | ptr[i + 5];
        frame.settings.emplace_back(key, value);

        LOG_TRACE << "  key=" << key << " value=" << value;
    }
    return frame;
}

static std::optional<WindowUpdateFrame> parseWindowUpdateFrame(
    const uint8_t *ptr,
    size_t length)
{
    if (length != 4)
    {
        LOG_ERROR << "Invalid window update frame length";
        return std::nullopt;
    }

    WindowUpdateFrame frame;
    // MSB is reserved for future use
    frame.windowSizeIncrement =
        (ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3]) & 0x7fffffff;
    LOG_TRACE << "Window update frame: windowSizeIncrement="
              << frame.windowSizeIncrement;
    return frame;
}

static size_t parseH2Frame(trantor::MsgBuffer *msg)
{
    if (msg->readableBytes() < 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame header";
        return 0;
    }

    uint8_t *ptr = (uint8_t *)msg->peek();

    // 24 bits length
    uint32_t length = ptr[0] << 16 | ptr[1] << 8 | ptr[2];
    uint8_t type = ptr[3];
    uint8_t flags = ptr[4];
    uint32_t streamId = ptr[5] << 24 | ptr[6] << 16 | ptr[7] << 8 | ptr[8];
    streamId &= 0x7fffffff;  // MSB is reserved for future use

    LOG_TRACE << "H2 frame: length=" << length << " type=" << (int)type
              << " flags=" << (int)flags << " streamId=" << streamId;
    if (msg->readableBytes() < length + 9)
    {
        LOG_TRACE << "Not enough bytes to parse H2 frame";
        return 0;
    }

    uint8_t *payload = ptr + 9;
    if (type == (uint8_t)H2FrameType::Settings)
    {
        auto settings = parseSettingsFrame(payload, length);
        if (!settings)
        {
            LOG_ERROR << "Failed to parse settings frame";
            return 0;
        }
    }
    else if (type == (uint8_t)H2FrameType::WindowUpdate)
    {
        auto windowUpdate = parseWindowUpdateFrame(payload, length);
        if (!windowUpdate)
        {
            LOG_ERROR << "Failed to parse window update frame";
            return 0;
        }
    }
    else
    {
        LOG_WARN << "Unsupported H2 frame type: " << (int)type;
    }

    msg->retrieve(length + 9);
    return length;
}

void Http2Transport::onRecvMessage(const trantor::TcpConnectionPtr &,
                                   trantor::MsgBuffer *msg)
{
    LOG_TRACE << "HTTP/2 message received:";
    dump_hex_beautiful(msg->peek(), msg->readableBytes());
    while (true)
    {
        size_t length = parseH2Frame(msg);
        if (length == 0)
            break;
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
