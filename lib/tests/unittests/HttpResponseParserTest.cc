#include <drogon/drogon_test.h>
#include "../../src/HttpResponseParser.h"
#include <trantor/utils/MsgBuffer.h>
#include <string_view>

using namespace drogon;

DROGON_TEST(HttpResponseParserRejectsMalformedStatusLine)
{
    for (std::string_view line : {"",
                                  " ",
                                  " HTTP/1.1 200 OK",
                                  "HTTP/1.1",
                                  "HTTP/1.1 ",
                                  "HTTP/1.1 20 OK",
                                  "HTTP/1.1 2000",
                                  "HTTP/1.1 abc OK",
                                  "X1 200 OK"})
    {
        HttpResponseParser parser(trantor::TcpConnectionPtr{});
        trantor::MsgBuffer buffer;
        buffer.append(line.data(), line.size());
        buffer.append("\r\n", 2);
        CHECK(!parser.parseResponse(&buffer));
    }
}

DROGON_TEST(HttpResponseParserAcceptsValidStatusLine)
{
    for (std::string_view line :
         {"HTTP/1.0 200 OK", "HTTP/1.1 204 ", "HTTP/1.1 200"})
    {
        HttpResponseParser parser(trantor::TcpConnectionPtr{});
        trantor::MsgBuffer buffer;
        buffer.append(line.data(), line.size());
        constexpr std::string_view suffix = "\r\nContent-Length: 0\r\n\r\n";
        buffer.append(suffix.data(), suffix.size());
        CHECK(parser.parseResponse(&buffer));
        CHECK(parser.gotAll());
    }
}

DROGON_TEST(HttpResponseParserLimitsUpgradeHeaders)
{
    HttpResponseParser parser(trantor::TcpConnectionPtr{});
    parser.setForHeadMethod();
    parser.setMaxHeaderSize(64);
    trantor::MsgBuffer buffer;
    buffer.append("HTTP/1.1 101 Switching Protocols\r\n", 34);
    CHECK(parser.parseResponse(&buffer));
    buffer.append("X-Long: ", 8);
    buffer.append(std::string(64, 'a'));
    CHECK(!parser.parseResponse(&buffer));
}

DROGON_TEST(HttpResponseParserUpgradeLeavesFrameInBuffer)
{
    HttpResponseParser parser(trantor::TcpConnectionPtr{});
    parser.setForHeadMethod();
    parser.setMaxHeaderSize(64);
    trantor::MsgBuffer buffer;
    constexpr std::string_view headers =
        "HTTP/1.1 101 Switching Protocols\r\nContent-Length: 1000000\r\n\r\n";
    buffer.append(headers.data(), headers.size());
    buffer.append(std::string(100, 'x'));
    CHECK(parser.parseResponse(&buffer));
    CHECK(parser.gotAll());
    CHECK(buffer.readableBytes() == 100);
}

DROGON_TEST(HttpResponseParserRejectsSignedContentLength)
{
    for (std::string_view len : {"-1", "+1"})
    {
        HttpResponseParser parser(trantor::TcpConnectionPtr{});
        trantor::MsgBuffer buffer;
        constexpr std::string_view prefix =
            "HTTP/1.1 200 OK\r\nContent-Length: ";
        buffer.append(prefix.data(), prefix.size());
        buffer.append(len.data(), len.size());
        buffer.append("\r\n\r\n", 4);
        CHECK(!parser.parseResponse(&buffer));
    }
}
