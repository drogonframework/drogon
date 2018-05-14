#include "TestController.h"
void TestController::asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)
{
    std::unique_ptr<HttpResponse> resp=std::unique_ptr<HttpResponse>(HttpResponse::newHttpResponse());
    LOG_DEBUG<<"!!!!!!!!!!1";
    resp->setStatusCode(HttpResponse::k200Ok);

    resp->setContentTypeCode(CT_TEXT_HTML);

    resp->setBody("hello!!!");
    callback(*resp);
}