#include "BenchmarkCtrl.h"
void BenchmarkCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // write your application logic here
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(0);
    callback(resp);
}
