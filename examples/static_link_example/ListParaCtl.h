#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class ListParaCtl:public drogon::HttpSimpleController<ListParaCtl>
{
public:
    virtual void asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)override;
    PATH_LIST_BEGIN
    //list path definations here;
    //PATH_ADD("/path","filter1","filter2",...);
    PATH_ADD("/listpara");
    PATH_LIST_END
};
