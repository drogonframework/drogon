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
}
