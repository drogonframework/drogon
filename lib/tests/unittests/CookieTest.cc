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
}
