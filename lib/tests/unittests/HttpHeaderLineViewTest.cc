#include <drogon/drogon_test.h>
#include "../../src/HttpRequestImpl.h"
#include "../../src/HttpResponseImpl.h"

using namespace drogon;

DROGON_TEST(HttpHeaderLineView)
{
    HttpRequestImpl request(nullptr);
    CHECK(request.addHeader("X-Test: \t hello \t"));
    CHECK(request.getHeaderBy("x-test") == "hello");
    CHECK(request.addHeader("Cookie: session=abc; theme=dark"));
    CHECK(request.getCookie("session") == "abc");
    CHECK(request.getCookie("theme") == "dark");
    // Quotes do not protect ';' in the Cookie header (RFC 6265, 5.4).
    CHECK(request.addHeader("Cookie: quoted=\"a;b\"; ignored; count=2"));
    CHECK(request.getCookie("quoted") == "\"a");
    CHECK(request.getCookie("count") == "2");
    CHECK(request.addHeader("Cookie: x=\"; session=victim"));
    CHECK(request.getCookie("session") == "victim");
    CHECK(!request.addHeader(""));
    CHECK(!request.addHeader("invalid"));

    HttpResponseImpl response;
    CHECK(response.addHeader("X-Test: \t hello \t"));
    CHECK(response.getHeaderBy("x-test") == "hello");
    CHECK(response.addHeader(
        "Set-Cookie: sid=abc; Path=/; Max-Age=3600; HttpOnly"));
    MANDATE(response.getCookies().count("sid") == 1);
    const auto &cookie = response.getCookies().at("sid");
    CHECK(cookie.path() == "/");
    CHECK(cookie.maxAge() == 3600);
    CHECK(cookie.isHttpOnly());
    CHECK(response.addHeader("Set-Cookie: ; sid2=def; Secure"));
    CHECK(response.getCookies().count("sid2") == 1);
    CHECK(!response.addHeader(""));
    CHECK(!response.addHeader("invalid"));
}
