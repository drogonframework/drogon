#include <drogon/drogon_test.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/MsgBuffer.h>
#include "../../lib/src/HttpRequestImpl.h"

using namespace drogon;

// Helper: parse a method string through HttpRequestImpl::setMethod
static std::pair<bool, HttpMethod> parseMethod(const std::string &str)
{
    HttpRequestImpl req(nullptr);
    bool ok = req.setMethod(str.data(), str.data() + str.size());
    return {ok, req.method()};
}

DROGON_TEST(StandardHttpMethods)
{
    auto [ok, m] = parseMethod("GET");
    CHECK(ok);
    CHECK(m == Get);

    std::tie(ok, m) = parseMethod("POST");
    CHECK(ok);
    CHECK(m == Post);

    std::tie(ok, m) = parseMethod("PUT");
    CHECK(ok);
    CHECK(m == Put);

    std::tie(ok, m) = parseMethod("DELETE");
    CHECK(ok);
    CHECK(m == Delete);

    std::tie(ok, m) = parseMethod("HEAD");
    CHECK(ok);
    CHECK(m == Head);

    std::tie(ok, m) = parseMethod("OPTIONS");
    CHECK(ok);
    CHECK(m == Options);

    std::tie(ok, m) = parseMethod("PATCH");
    CHECK(ok);
    CHECK(m == Patch);
}

DROGON_TEST(WebDavMethods)
{
    auto [ok, m] = parseMethod("PROPFIND");
    CHECK(ok);
    CHECK(m == Propfind);

    std::tie(ok, m) = parseMethod("MKCOL");
    CHECK(ok);
    CHECK(m == Mkcol);

    std::tie(ok, m) = parseMethod("COPY");
    CHECK(ok);
    CHECK(m == Copy);

    std::tie(ok, m) = parseMethod("MOVE");
    CHECK(ok);
    CHECK(m == Move);
}

DROGON_TEST(WebDavMethodStrings)
{
    CHECK(to_string_view(Propfind) == "PROPFIND");
    CHECK(to_string_view(Mkcol) == "MKCOL");
    CHECK(to_string_view(Copy) == "COPY");
    CHECK(to_string_view(Move) == "MOVE");
}

// Helper: serialize a request and return the first line (method + path)
static std::string serializeMethod(HttpMethod method)
{
    HttpRequestImpl req(nullptr);
    req.setMethod(method);
    req.setPath("/test");
    trantor::MsgBuffer buf;
    req.appendToBuffer(&buf);
    std::string result(buf.peek(), buf.readableBytes());
    // Return just up to the first space after the method
    auto pos = result.find(' ');
    return result.substr(0, pos);
}

DROGON_TEST(MethodSerialization)
{
    CHECK(serializeMethod(Get) == "GET");
    CHECK(serializeMethod(Post) == "POST");
    CHECK(serializeMethod(Put) == "PUT");
    CHECK(serializeMethod(Delete) == "DELETE");
    CHECK(serializeMethod(Head) == "HEAD");
    CHECK(serializeMethod(Options) == "OPTIONS");
    CHECK(serializeMethod(Patch) == "PATCH");
    CHECK(serializeMethod(Propfind) == "PROPFIND");
    CHECK(serializeMethod(Mkcol) == "MKCOL");
    CHECK(serializeMethod(Copy) == "COPY");
    CHECK(serializeMethod(Move) == "MOVE");
}

DROGON_TEST(InvalidMethodsRejected)
{
    auto [ok, m] = parseMethod("INVALID");
    CHECK(!ok);
    CHECK(m == Invalid);

    std::tie(ok, m) = parseMethod("LOCK");
    CHECK(!ok);
    CHECK(m == Invalid);

    std::tie(ok, m) = parseMethod("");
    CHECK(!ok);
    CHECK(m == Invalid);

    std::tie(ok, m) = parseMethod("G");
    CHECK(!ok);
    CHECK(m == Invalid);
}
