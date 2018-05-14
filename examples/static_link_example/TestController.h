#pragma once

#include <drogon/HttpSimpleController.h>
using namespace drogon;

class TestController:public drogon::HttpSimpleController<TestController>
{
public:
    //TestController(){}
    virtual void asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)override;

    PATH_LIST_BEGIN
    PATH_ADD("/");
    PATH_ADD("/Test","nonFilter");
    PATH_ADD("/tpost","drogon::PostFilter");
    PATH_LIST_END
};