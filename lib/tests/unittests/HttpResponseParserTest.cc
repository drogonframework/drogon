#include <drogon/drogon_test.h>
#include <drogon/HttpTypes.h>
#include "../../src/HttpResponseParser.h"
#include "../../src/HttpResponseImpl.h"
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

namespace
{
struct ParserFixture
{
    HttpResponseParser parser{nullptr};
    trantor::MsgBuffer buffer;

    bool feed(const std::string &raw)
    {
        buffer.append(raw);
        return parser.parseResponse(&buffer);
    }
};
}  // namespace

DROGON_TEST(HttpResponseRejectsMalformedContentLength)
{
    for (const char *value : {"", "-1", "+10", "10x", "1 0", "0x10"})
    {
        ParserFixture f;
        std::string raw = "HTTP/1.1 200 OK\r\ncontent-length: ";
        raw += value;
        raw += "\r\n\r\nbody";
        CHECK(!f.feed(raw));
    }
}

DROGON_TEST(HttpResponseRejectsOverflowingContentLength)
{
    ParserFixture f;
    CHECK(!f.feed(
        "HTTP/1.1 200 OK\r\ncontent-length: 99999999999999999999999\r\n\r\n"));
}

DROGON_TEST(HttpResponseRejectsRepeatedContentLength)
{
    ParserFixture f;
    CHECK(!f.feed(
        "HTTP/1.1 200 OK\r\ncontent-length: 1\r\ncontent-length: 1\r\n\r\nX"));
}

DROGON_TEST(HttpResponseRejectsContentLengthWithTransferEncoding)
{
    ParserFixture f;
    CHECK(
        !f.feed("HTTP/1.1 200 OK\r\ncontent-length: 5\r\n"
                "transfer-encoding: chunked\r\n\r\n"));
}

DROGON_TEST(HttpResponseAcceptsValidContentLength)
{
    ParserFixture f;
    CHECK(f.feed("HTTP/1.1 200 OK\r\ncontent-length: 5\r\n\r\nhello"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->getBody() == "hello");
    CHECK(f.buffer.readableBytes() == 0);
}

DROGON_TEST(HttpResponseChunkedCodingNameIsCaseInsensitive)
{
    ParserFixture f;
    CHECK(
        f.feed("HTTP/1.1 200 OK\r\ntransfer-encoding: Chunked\r\n\r\n"
               "5\r\nhello\r\n0\r\n\r\n"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->getBody() == "hello");
    CHECK(f.buffer.readableBytes() == 0);
}

DROGON_TEST(HttpResponseChunkedCodingAcceptsParameters)
{
    ParserFixture f;
    CHECK(f.feed(
        "HTTP/1.1 200 OK\r\ntransfer-encoding: gzip, Chunked;foo=bar\r\n\r\n"
        "5\r\nhello\r\n0\r\n\r\n"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->getBody() == "hello");
}

DROGON_TEST(HttpResponseSkipsChunkedTrailers)
{
    ParserFixture f;
    CHECK(
        f.feed("HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n"
               "5\r\nhello\r\n0\r\nX-Trailer: value\r\n\r\n"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->getBody() == "hello");
    // The trailer section must be consumed, or its tail is parsed as the
    // beginning of the next response.
    CHECK(f.buffer.readableBytes() == 0);
}

DROGON_TEST(HttpResponseBodylessStatusIgnoresContentLength)
{
    for (const char *status : {"304 Not Modified", "204 No Content"})
    {
        ParserFixture f;
        std::string raw = "HTTP/1.1 ";
        raw += status;
        raw += "\r\ncontent-length: 100\r\n\r\n";
        CHECK(f.feed(raw));
        CHECK(f.parser.gotAll());
        CHECK(f.parser.responseImpl()->getBodyLength() == 0);
    }
}

DROGON_TEST(HttpResponseSkipsInformationalResponse)
{
    ParserFixture f;
    CHECK(
        f.feed("HTTP/1.1 100 Continue\r\n\r\n"
               "HTTP/1.1 200 OK\r\ncontent-length: 2\r\n\r\nok"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->statusCode() == k100Continue);

    // The caller resets and parses the final response from the same buffer.
    f.parser.reset();
    CHECK(f.parser.parseResponse(&f.buffer));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->statusCode() == k200OK);
    CHECK(f.parser.responseImpl()->getBody() == "ok");
}

DROGON_TEST(HttpResponseForHeadMethodHasNoBody)
{
    ParserFixture f;
    f.parser.setForHeadMethod();
    // Conflicting framing headers must not matter: a HEAD response never has
    // a body (RFC 9112 section 6.3).
    CHECK(
        f.feed("HTTP/1.1 200 OK\r\ncontent-length: 5\r\n"
               "transfer-encoding: chunked\r\n\r\n"));
    CHECK(f.parser.gotAll());
    CHECK(f.parser.responseImpl()->getBodyLength() == 0);
}
