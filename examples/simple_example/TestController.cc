#include "TestController.h"
using namespace example;
void TestController::asyncHandleHttpRequest(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback)
{
    //write your application logic here
    LOG_WARN << req->matchedPathPattern();
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(0);
    callback(resp);
}
