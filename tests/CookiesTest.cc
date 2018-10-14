#include <drogon/Cookie.h>
#include <iostream>
int main()
{
    drogon::Cookie cookie1("test", "1");
    std::cout << cookie1.cookieString();
    drogon::Cookie cookie2("test", "2");
    cookie2.setEnsure(true);
    std::cout << cookie2.cookieString();
    drogon::Cookie cookie3("test", "3");
    cookie3.setDomain("drogon.org");
    cookie3.setExpiresDate(trantor::Date::date().after(3600 * 24));
    std::cout << cookie3.cookieString();
}