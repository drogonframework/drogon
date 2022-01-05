#include "BeginAdviceTest.h"

std::string BeginAdviceTest::content_ = "Default content";

void BeginAdviceTest::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(content_);
    callback(resp);
}
