#include <drogon/drogon_test.h>

#include "../../src/WebSocketConnectionImpl.h"

static std::string makeFrame(size_t length,
                             bool masked,
                             bool finished,
                             unsigned char opcode)
{
    std::string frame;
    frame.push_back(static_cast<char>((finished ? 0x80 : 0) | opcode));
    const unsigned char maskBit = masked ? 0x80 : 0;
    if (length <= 125)
    {
        frame.push_back(static_cast<char>(maskBit | length));
    }
    else if (length <= 0xffff)
    {
        frame.push_back(static_cast<char>(maskBit | 126));
        frame.push_back(static_cast<char>((length >> 8) & 0xff));
        frame.push_back(static_cast<char>(length & 0xff));
    }
    else
    {
        frame.push_back(static_cast<char>(maskBit | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back(static_cast<char>((length >> shift) & 0xff));
    }

    const unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    if (masked)
        frame.append(reinterpret_cast<const char *>(mask), sizeof(mask));
    for (size_t i = 0; i < length; ++i)
        frame.push_back(static_cast<char>('x' ^ (masked ? mask[i % 4] : 0)));
    return frame;
}

static bool parseFrame(drogon::WebSocketMessageParser &parser,
                       const std::string &frame)
{
    trantor::MsgBuffer buffer;
    buffer.append(frame.data(), frame.size());
    return parser.parse(&buffer);
}

DROGON_TEST(WebSocketMessageSizeAndMaskValidation)
{
    drogon::WebSocketMessageParser fragmented;
    CHECK(parseFrame(fragmented, makeFrame(70 * 1024, true, false, 2)));
    CHECK(!parseFrame(fragmented, makeFrame(70 * 1024, true, true, 0)));

    drogon::WebSocketMessageParser serverParser;
    CHECK(!parseFrame(serverParser, makeFrame(1, false, true, 1)));

    drogon::WebSocketMessageParser clientParser(false);
    CHECK(parseFrame(clientParser, makeFrame(1, false, true, 1)));
    std::string message;
    drogon::WebSocketMessageType type;
    CHECK(clientParser.gotAll(message, type));
    CHECK(message == "x");

    trantor::MsgBuffer invalidLength;
    const unsigned char header[] = {
        0x82, 0xff, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    invalidLength.append(reinterpret_cast<const char *>(header),
                         sizeof(header));
    drogon::WebSocketMessageParser invalidLengthParser;
    CHECK(!invalidLengthParser.parse(&invalidLength));
}
