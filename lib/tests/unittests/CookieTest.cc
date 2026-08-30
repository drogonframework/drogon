#include <drogon/Cookie.h>
#include <drogon/drogon_test.h>

DROGON_TEST(CookieTest)
{
    drogon::Cookie cookie1("test", "1");
    CHECK(cookie1.cookieString() == "Set-Cookie: test=1; HttpOnly\r\n");

    drogon::Cookie cookie2("test", "2");
    cookie2.setSecure(true);
    CHECK(cookie2.cookieString() == "Set-Cookie: test=2; Secure; HttpOnly\r\n");

    drogon::Cookie cookie3("test", "3");
    cookie3.setDomain("drogon.org");
    cookie3.setExpiresDate(trantor::Date(1621561557000000L));
    CHECK(cookie3.cookieString() ==
          "Set-Cookie: test=3; Expires=Fri, 21 May 2021 01:45:57 GMT; "
          "Domain=drogon.org; HttpOnly\r\n");

    drogon::Cookie cookie4("test", "4");
    cookie4.setMaxAge(3600);
    cookie4.setSameSite(drogon::Cookie::SameSite::kStrict);
    CHECK(cookie4.cookieString() ==
          "Set-Cookie: test=4; Max-Age=3600; "
          "SameSite=Strict; HttpOnly\r\n");
    drogon::Cookie cookie5("test", "5");
    cookie5.setSameSite(drogon::Cookie::SameSite::kNone);
    CHECK(cookie5.cookieString() ==
          "Set-Cookie: test=5; "
          "SameSite=None; Secure; HttpOnly\r\n");

    CHECK(drogon::Cookie::SameSite::kLax ==
          drogon::Cookie::convertString2SameSite("Lax"));
    CHECK(drogon::Cookie::SameSite::kStrict ==
          drogon::Cookie::convertString2SameSite("Strict"));
    CHECK(drogon::Cookie::SameSite::kNone ==
          drogon::Cookie::convertString2SameSite("None"));

    // Test for Partitioned attribute
    drogon::Cookie cookie6("test", "6");
    cookie6.setPartitioned(true);
    CHECK(cookie6.cookieString() ==
          "Set-Cookie: test=6; Secure; HttpOnly; Partitioned\r\n");
    // Test that partitioned attribute  automatically sets secure
    drogon::Cookie cookie7("test", "7");
    cookie7.setPartitioned(true);
    CHECK(cookie7.isSecure() == true);
    // Test other attributes
    drogon::Cookie cookie8("test", "8");
    cookie8.setPartitioned(true);
    cookie8.setDomain("drogon.org");
    cookie8.setMaxAge(3600);
    CHECK(cookie8.cookieString() ==
          "Set-Cookie: test=8; Max-Age=3600; Domain=drogon.org; Secure; "
          "HttpOnly; Partitioned\r\n");
    // Teset Partitioned and SameSite can coexist
    drogon::Cookie cookie9("test", "9");
    cookie9.setPartitioned(true);
    cookie9.setSameSite(drogon::Cookie::SameSite::kLax);
    CHECK(
        cookie9.cookieString() ==
        "Set-Cookie: test=9; SameSite=Lax; Secure; HttpOnly; Partitioned\r\n");
}
