/**
 *
 *  HttpRequestForwardCacheBodyTest.cc
 *
 *  Regression test for https://github.com/drogonframework/drogon/issues/2482
 *
 *  When an inbound HTTP request body exceeds clientMaxMemoryBodySize_, the
 *  body is spilled to a CacheFile (HttpRequestImpl::cacheFilePtr_) instead of
 *  HttpRequestImpl::content_. HttpRequestImpl::appendToBuffer() is used by
 *  HttpClientImpl when forwarding the request to a downstream server, and it
 *  must include the cached body in the serialized buffer (and in the
 *  Content-Length header).
 */
#include <drogon/drogon_test.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/MsgBuffer.h>
#include "../../lib/src/HttpRequestImpl.h"
#include "../../lib/src/CacheFile.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#ifdef _WIN32
#include <process.h>
#define DROGON_TEST_GETPID _getpid
#else
#include <unistd.h>
#define DROGON_TEST_GETPID getpid
#endif

using namespace drogon;

// Test-only access shim that befriends HttpRequestImpl in the header. It
// lets the test inject a CacheFile-backed body, simulating the state the
// request parser leaves the request in when the body has been spilled to
// disk.
class HttpRequestImplCacheFileTestAccess
{
  public:
    static void installCacheFile(HttpRequestImpl &req,
                                 std::unique_ptr<CacheFile> cf)
    {
        req.cacheFilePtr_ = std::move(cf);
    }
};

namespace
{
std::string makeTmpPath(const std::string &tag)
{
    // tmpnam-style path under the OS temp dir, but using fixed prefixes so we
    // can run repeatedly without name collisions.
#ifdef _WIN32
    const char *base = std::getenv("TEMP");
    if (!base || !*base)
        base = std::getenv("TMP");
    if (!base || !*base)
        base = "C:/Windows/Temp";
    std::string out = base;
    out += "/drogon-issue2482-";
#else
    std::string out = "/tmp/drogon-issue2482-";
#endif
    out += tag;
    out += "-";
    out += std::to_string(DROGON_TEST_GETPID());
    out += "-";
    out += std::to_string(reinterpret_cast<uintptr_t>(&out));
    return out;
}

std::unique_ptr<CacheFile> makeCacheFileWith(const std::string &payload,
                                             const std::string &tag)
{
    auto path = makeTmpPath(tag);
    auto cf = std::make_unique<CacheFile>(path, /*autoDelete=*/true);
    cf->append(payload);
    return cf;
}

// Find a header value in a serialized HTTP request buffer, matching
// case-insensitively for the field name and trimming the value.
std::string findHeader(const std::string &raw, const std::string &name)
{
    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return {};
    std::string headers = raw.substr(0, headerEnd + 2);
    std::string lname = name;
    for (auto &c : lname)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

    size_t pos = 0;
    while (pos < headers.size())
    {
        auto eol = headers.find("\r\n", pos);
        if (eol == std::string::npos)
            break;
        std::string line = headers.substr(pos, eol - pos);
        pos = eol + 2;
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string field = line.substr(0, colon);
        for (auto &c : field)
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (field == lname)
        {
            std::string value = line.substr(colon + 1);
            size_t s = 0;
            while (s < value.size() &&
                   ::isspace(static_cast<unsigned char>(value[s])))
                ++s;
            size_t e = value.size();
            while (e > s && ::isspace(static_cast<unsigned char>(value[e - 1])))
                --e;
            return value.substr(s, e - s);
        }
    }
    return {};
}

std::string extractBody(const std::string &raw)
{
    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return {};
    return raw.substr(headerEnd + 4);
}
}  // namespace

DROGON_TEST(HttpRequestForwardCacheBody_BodyEmpty)
{
    // Sanity check: with neither content_ nor a cache file, a non-body method
    // must not emit a content-length header.
    HttpRequestImpl req(nullptr);
    req.setMethod(Get);
    req.setVersion(Version::kHttp11);
    req.setPath("/foo");

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());
    CHECK(findHeader(raw, "content-length").empty());
    CHECK(extractBody(raw).empty());
}

DROGON_TEST(HttpRequestForwardCacheBody_InMemoryStillForwarded)
{
    // Regression guard for the existing (small) body path.
    const std::string body = "small-in-memory-body";
    HttpRequestImpl req(nullptr);
    req.setMethod(Post);
    req.setVersion(Version::kHttp11);
    req.setPath("/echo");
    req.setBody(body);

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(findHeader(raw, "content-length") == std::to_string(body.size()));
    CHECK(extractBody(raw) == body);
}

DROGON_TEST(HttpRequestForwardCacheBody_CachedBodyForwarded)
{
    // The bug from #2482: a body sitting in cacheFilePtr_ must be serialized
    // by appendToBuffer() and accounted for in content-length.
    std::string body;
    body.reserve(200 * 1024);
    for (int i = 0; i < 200 * 1024; ++i)
        body.push_back(static_cast<char>('A' + (i % 26)));

    HttpRequestImpl req(nullptr);
    req.setMethod(Post);
    req.setVersion(Version::kHttp11);
    req.setPath("/forward");
    HttpRequestImplCacheFileTestAccess::installCacheFile(
        req, makeCacheFileWith(body, "cached"));

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(findHeader(raw, "content-length") == std::to_string(body.size()));
    CHECK(extractBody(raw) == body);
}

DROGON_TEST(HttpRequestForwardCacheBody_CachedBodyExactBoundary)
{
    // Exactly the default clientMaxMemoryBodySize boundary. Make sure we
    // correctly emit the body even when its size is on the round number.
    const size_t kSize = 64 * 1024;
    std::string body(kSize, 'x');
    HttpRequestImpl req(nullptr);
    req.setMethod(Put);
    req.setVersion(Version::kHttp11);
    req.setPath("/upload");
    HttpRequestImplCacheFileTestAccess::installCacheFile(
        req, makeCacheFileWith(body, "boundary"));

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(findHeader(raw, "content-length") == std::to_string(kSize));
    CHECK(extractBody(raw).size() == kSize);
    CHECK(extractBody(raw) == body);
}

DROGON_TEST(HttpRequestForwardCacheBody_CachedBodyBinarySafe)
{
    // The cached body must survive serialization byte-for-byte, including
    // NULs and high bytes.
    std::string body;
    body.resize(70 * 1024);
    for (size_t i = 0; i < body.size(); ++i)
        body[i] = static_cast<char>(i & 0xff);

    HttpRequestImpl req(nullptr);
    req.setMethod(Post);
    req.setVersion(Version::kHttp11);
    req.setPath("/binary");
    HttpRequestImplCacheFileTestAccess::installCacheFile(
        req, makeCacheFileWith(body, "binary"));

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(findHeader(raw, "content-length") == std::to_string(body.size()));
    CHECK(extractBody(raw) == body);
}

DROGON_TEST(HttpRequestForwardCacheBody_PassThroughKeepsBody)
{
    // passThrough_ skips parameter/content-length emission, but the body
    // bytes from a cache file must still appear in the serialized buffer.
    const std::string body(80 * 1024, 'p');
    HttpRequestImpl req(nullptr);
    req.setMethod(Post);
    req.setVersion(Version::kHttp11);
    req.setPath("/pass");
    req.setPassThrough(true);
    HttpRequestImplCacheFileTestAccess::installCacheFile(
        req, makeCacheFileWith(body, "passthrough"));
    // passThrough mode expects callers to preserve the original
    // content-length header.
    req.addHeader("content-length", std::to_string(body.size()));

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(extractBody(raw) == body);
}

DROGON_TEST(HttpRequestForwardCacheBody_CachedBodyWithCustomHeaders)
{
    // Headers and cookies must continue to be serialized in addition to the
    // cached body.
    const std::string body(100 * 1024, 'h');
    HttpRequestImpl req(nullptr);
    req.setMethod(Post);
    req.setVersion(Version::kHttp11);
    req.setPath("/with-headers");
    req.addHeader("X-Trace-Id", "abc-123");
    req.addCookie("session", "deadbeef");
    HttpRequestImplCacheFileTestAccess::installCacheFile(
        req, makeCacheFileWith(body, "headers"));

    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string raw(buf.peek(), buf.readableBytes());

    CHECK(findHeader(raw, "content-length") == std::to_string(body.size()));
    CHECK(findHeader(raw, "x-trace-id") == "abc-123");
    CHECK(raw.find("cookie: session=deadbeef") != std::string::npos);
    CHECK(extractBody(raw) == body);
}
