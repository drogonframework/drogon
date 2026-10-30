/**
 *  Regression tests for RFC 9112 response body framing in
 *  HttpResponseParser. A lenient parse lets a client disagree with an
 *  intermediary about where a response body ends, which desynchronizes a
 *  keep-alive or pipelined connection.
 */
#include <drogon/drogon_test.h>
#include <drogon/HttpTypes.h>
#include "../../lib/src/HttpResponseParser.h"
#include "../../lib/src/HttpResponseImpl.h"
#include <trantor/utils/MsgBuffer.h>

#include <string>

using namespace drogon;

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
