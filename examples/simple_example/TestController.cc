#include "TestController.h"
using namespace example;
void TestController::asyncHandleHttpRequest(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback)
{
    //write your application logic here
    auto resp=HttpResponse::newHttpResponse();
    LOG_DEBUG<<"!!!!!!!!!!1";
    resp->setStatusCode(HttpResponse::k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody("hello!!!");
    callback(resp);
}