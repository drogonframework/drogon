#include "../lib/src/HttpUtils.h"
#include <drogon/Cookie.h>
#include <drogon/drogon_test.h>
using namespace drogon;

DROGON_TEST(IsValidHeader)
{
    CHECK(isValidHeader("X-Test", "abc"));
    CHECK(isValidHeader("X-Test", ""));
    CHECK(isValidHeader("X-Test", "a: b; c=d"));
    CHECK(!isValidHeader("X-Test", "abc\r\nX-Injected: 1"));
    CHECK(!isValidHeader("X-Test", "abc\r"));
    CHECK(!isValidHeader("X-Test", "abc\nX-Injected: 1"));
    CHECK(!isValidHeader("X-Test\r\nX-Injected", "abc"));
    CHECK(!isValidHeader("X-Test\n", "abc"));
}

DROGON_TEST(IsValidCookie)
{
    CHECK(isValidCookie("key", "value"));
    CHECK(isValidCookie("key", ""));
    CHECK(!isValidCookie("key", "value\r\nX-Injected: 1"));
    CHECK(!isValidCookie("key", "value\n"));
    CHECK(!isValidCookie("key\r", "value"));

    Cookie cookie("key", "value");
    cookie.setDomain("example.com");
    cookie.setPath("/");
    CHECK(isValidCookie(cookie));

    cookie.setValue("value\r\nX-Injected: 1");
    CHECK(!isValidCookie(cookie));

    cookie.setValue("value");
    cookie.setDomain("example.com\r\nX-Injected: 1");
    CHECK(!isValidCookie(cookie));

    cookie.setDomain("example.com");
    cookie.setPath("/\nX-Injected: 1");
    CHECK(!isValidCookie(cookie));
}
