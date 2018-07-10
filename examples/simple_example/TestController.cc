#include "TestController.h"
using namespace example;
void TestController::asyncHandleHttpRequest(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback)
{
    //write your application logic here
    auto resp=HttpResponse::newHttpResponse();
    LOG_DEBUG<<"!!!!!!!!!!1";
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody("hello!!!");
    callback(*resp);
}