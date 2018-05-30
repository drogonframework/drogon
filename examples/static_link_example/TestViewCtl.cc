#include "TestViewCtl.h"
void TestViewCtl::asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)
{
    //write your application logic here
    drogon::HttpViewData data;
    data.insert("title",std::string("TestView"));
    std::unique_ptr<HttpResponse> res=std::unique_ptr<HttpResponse>(drogon::HttpResponse::newHttpViewResponse("TestViewClone",data));
    callback(*res);
}