#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
namespace example{
    class TestController:public drogon::HttpSimpleController<TestController>
    {
    public:
        virtual void asyncHandleHttpRequest(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback)override;
        PATH_LIST_BEGIN
        //list path definations here;
        //PATH_ADD("/path","filter1","filter2",...);
        PATH_ADD("/");
        PATH_ADD("/Test","nonFilter");
        PATH_ADD("/tpost","drogon::PostFilter");
        PATH_ADD("/slow","TimeFilter");
        PATH_LIST_END
    };
}
