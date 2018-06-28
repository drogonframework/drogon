#include "TestViewCtl.h"
void TestViewCtl::asyncHandleHttpRequest(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback)
{
    //write your application logic here
    drogon::HttpViewData data;
    data.insert("title",std::string("TestView"));
    auto res=drogon::HttpResponse::newHttpViewResponse("TestView",data);
    callback(*res);
}