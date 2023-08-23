#include "CustomCtrl.h"

// add definition of your processing function here

void CustomCtrl::hello(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       const std::string &userName) const
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("<P>" + greetings_ + ", " + userName + "</P>");
    callback(resp);
}
