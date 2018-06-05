#include "ListParaCtl.h"
void ListParaCtl::asyncHandleHttpRequest(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback)
{
    //write your application logic here
    HttpViewData data;
    data.insert("title",std::string("list parameters"));
    data.insert("parameters",req.getPremeter());
    auto res=drogon::HttpResponse::newHttpViewResponse("ListParaView.csp",data);
    callback(*res);
}