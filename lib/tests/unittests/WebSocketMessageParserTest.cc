#include <drogon/drogon_test.h>
#include "../../src/WebSocketConnectionImpl.h"
#include <trantor/utils/MsgBuffer.h>
#include <cassert>
#include <string_view>

using namespace drogon;

namespace
{
void appendFrame(trantor::MsgBuffer &buffer,
                 unsigned char firstByte,
                 std::string_view payload,
                 bool masked = true)
{
    assert(payload.size() <= 125);
    const char header[] = {static_cast<char>(firstByte),
                           static_cast<char>(payload.size() |
                                             (masked ? 0x80 : 0))};
    buffer.append(header, sizeof(header));
    if (masked)
    {
        const char mask[] = {0x11, 0x22, 0x33, 0x44};
        buffer.append(mask, sizeof(mask));
        for (size_t i = 0; i < payload.size(); ++i)
        {
            const char byte = payload[i] ^ mask[i % sizeof(mask)];
            buffer.append(&byte, 1);
        }
    }
    else
    {
        buffer.append(payload.data(), payload.size());
    }
}
}  // namespace

DROGON_TEST(WebSocketControlFrameBetweenDataFragments)
{
    WebSocketMessageParser parser;
    trantor::MsgBuffer buffer;
    const std::string firstFragment(125, 'a');
    appendFrame(buffer, 0x01, firstFragment);  // unfinished text
    appendFrame(buffer, 0x89, "P");            // ping
    appendFrame(buffer, 0x80, "END");          // final continuation

    CHECK(parser.parse(&buffer));
    std::string message;
    WebSocketMessageType type;
    CHECK(parser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Ping);
    CHECK(message == "P");

    CHECK(parser.parse(&buffer));
    message.clear();
    CHECK(parser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Text);
    CHECK(message == firstFragment + "END");
    CHECK(buffer.readableBytes() == 0);
}

DROGON_TEST(WebSocketControlFramePreservesFragmentStateAcrossReads)
{
    WebSocketMessageParser parser;
    trantor::MsgBuffer buffer;
    appendFrame(buffer, 0x02, "A", false);  // unfinished binary
    CHECK(parser.parse(&buffer, false));
    std::string message;
    WebSocketMessageType type;
    CHECK(!parser.gotAll(message, type));

    appendFrame(buffer, 0x8a, "B", false);  // pong
    CHECK(parser.parse(&buffer, false));
    CHECK(parser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Pong);
    CHECK(message == "B");

    message.clear();
    appendFrame(buffer, 0x80, "C", false);  // final continuation
    CHECK(parser.parse(&buffer, false));
    CHECK(parser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Binary);
    CHECK(message == "AC");
}

DROGON_TEST(WebSocketRejectsInvalidFragmentSequence)
{
    WebSocketMessageParser parser;
    trantor::MsgBuffer buffer;
    appendFrame(buffer, 0x80, "orphan");
    CHECK(!parser.parse(&buffer));

    WebSocketMessageParser secondParser;
    trantor::MsgBuffer secondBuffer;
    appendFrame(secondBuffer, 0x01, "first");
    appendFrame(secondBuffer, 0x82, "second");
    CHECK(!secondParser.parse(&secondBuffer));
}

DROGON_TEST(WebSocketRejectsReservedBits)
{
    for (unsigned char reservedBit : {0x40, 0x20, 0x10})
    {
        WebSocketMessageParser parser;
        trantor::MsgBuffer buffer;
        appendFrame(buffer,
                    static_cast<unsigned char>(0x81 | reservedBit),
                    "A");
        CHECK(!parser.parse(&buffer));
    }
}

DROGON_TEST(WebSocketValidatesTextUTF8AcrossFragments)
{
    WebSocketMessageParser parser;
    trantor::MsgBuffer buffer;
    appendFrame(buffer, 0x01, "\xe2");
    appendFrame(buffer, 0x80, "\x82\xac");
    CHECK(parser.parse(&buffer));
    std::string message;
    WebSocketMessageType type;
    CHECK(parser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Text);
    CHECK(message == "\xe2\x82\xac");

    WebSocketMessageParser invalidParser;
    trantor::MsgBuffer invalidBuffer;
    appendFrame(invalidBuffer, 0x01, "\xc0");
    appendFrame(invalidBuffer, 0x80, "\xaf");
    CHECK(!invalidParser.parse(&invalidBuffer));

    WebSocketMessageParser singleFrameParser;
    trantor::MsgBuffer singleFrameBuffer;
    appendFrame(singleFrameBuffer, 0x81, "\xc0\xaf");
    CHECK(!singleFrameParser.parse(&singleFrameBuffer));
}

DROGON_TEST(WebSocketRejectsMalformedClosePayload)
{
    WebSocketMessageParser shortCloseParser;
    trantor::MsgBuffer shortCloseBuffer;
    appendFrame(shortCloseBuffer, 0x88, "\x03");
    CHECK(!shortCloseParser.parse(&shortCloseBuffer));

    WebSocketMessageParser invalidReasonParser;
    trantor::MsgBuffer invalidReasonBuffer;
    appendFrame(invalidReasonBuffer, 0x88, "\x03\xe8\xc0\xaf");
    CHECK(!invalidReasonParser.parse(&invalidReasonBuffer));

    WebSocketMessageParser validCloseParser;
    trantor::MsgBuffer validCloseBuffer;
    appendFrame(validCloseBuffer, 0x88, "\x03\xe8\xe2\x82\xac");
    CHECK(validCloseParser.parse(&validCloseBuffer));
    std::string message;
    WebSocketMessageType type;
    CHECK(validCloseParser.gotAll(message, type));
    CHECK(type == WebSocketMessageType::Close);
    CHECK(message == "\x03\xe8\xe2\x82\xac");
}
