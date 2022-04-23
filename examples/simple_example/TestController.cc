#include "TestController.h"
using namespace example;
void TestController::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // write your application logic here
    LOG_WARN << req->matchedPathPatternData();
    LOG_DEBUG << "index=" << threadIndex_.getThreadData();
    ++(threadIndex_.getThreadData());
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCodeAndCustomString(CT_TEXT_PLAIN,
                                            "content-type: plaintext\r\n");
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(20);
    callback(resp);
}
