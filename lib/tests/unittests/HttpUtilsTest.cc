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

DROGON_TEST(WebSocketHandshakeFields)
{
    CHECK(containsHttpToken("keep-alive, Upgrade", "upgrade"));
    CHECK(containsHttpToken("UPGRADE", "upgrade"));
    CHECK(!containsHttpToken("notupgrade", "upgrade"));
    CHECK(!containsHttpToken("keep-alive, upgraded", "upgrade"));
}

DROGON_TEST(IterateHeaderField)
{
    auto fields = iterateHeaderField(" keep-alive , Upgrade, \"a,b\", ");
    CHECK(fields.next() == "keep-alive");
    CHECK(fields.next() == "Upgrade");
    CHECK(fields.next() == "\"a,b\"");
    CHECK(fields.next() == "");
    CHECK(!fields.next());

    auto empty = iterateHeaderField("");
    CHECK(empty.next() == "");
    CHECK(!empty.next());
}

DROGON_TEST(IterateHeaderParameters)
{
    auto [master, parameters] =
        iterateHeaderParameters(" text/plain ; charset = utf-8; title=\"a;b\"");
    CHECK(master == "text/plain");
    auto first = parameters.next();
    CHECK(first.has_value());
    CHECK(first->first == "charset");
    CHECK(first->second == "utf-8");
    auto second = parameters.next();
    CHECK(second.has_value());
    CHECK(second->first == "title");
    CHECK(second->second == "\"a;b\"");
    CHECK(!parameters.next());
}

DROGON_TEST(CanonicalRequestPath)
{
    using namespace std::string_literals;
    for (const auto *path : {"/",
                             "/a",
                             "/a/",
                             "/a/b.html",
                             "/.well-known/x",
                             "/a/..b",
                             "/a/b.",
                             "/*"})
    {
        CHECK(canonicalizePath(path) == path);
    }

    const std::pair<std::string, std::string> rewritten[] = {
        {"//a", "/a"},
        {"/./a", "/a"},
        {"/a//b/", "/a/b/"},
        {"/a/./b", "/a/b"},
        {"/x/../a", "/a"},
        {"/a/.", "/a/"},
        {"/a/b/..", "/a/"},
        {"/a/..", "/"},
        {"///", "/"},
    };
    for (const auto &[in, expected] : rewritten)
    {
        CHECK(canonicalizePath(in) == expected);
        // Canonicalizing is idempotent.
        CHECK(canonicalizePath(expected) == expected);
    }

    for (const auto &bad : {"/.."s,
                            "/../a"s,
                            "/a/../.."s,
                            "//../a"s,
                            "/a\0.html"s,
                            "/a/\0/../b"s,
                            "/./\0"s})
    {
        CHECK(!canonicalizePath(bad));
    }
#ifdef _WIN32
    CHECK(!canonicalizePath("/C:/Windows/win.ini"));
    CHECK(!canonicalizePath("/..\\a"));
    CHECK(canonicalizePath("/a\\b") == "/a/b");
#else
    // A backslash is an ordinary character outside Windows.
    CHECK(canonicalizePath("/..\\a") == "/..\\a");
#endif
}
