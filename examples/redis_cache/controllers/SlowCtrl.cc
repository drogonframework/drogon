#include "SlowCtrl.h"

void SlowCtrl::hello(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string &&userid)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("hello, " + userid);
    callback(resp);
}
