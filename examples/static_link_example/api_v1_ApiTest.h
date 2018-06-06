#pragma once
#include <drogon/HttpApiController.h>
using namespace drogon;
namespace api
{
    namespace v1
    {
        class ApiTest:public drogon::HttpApiController<ApiTest>
        {
        METHOD_LIST_BEGIN
        //use METHOD_ADD to add your custom processing function here;
        METHOD_ADD(ApiTest::get,"/{2}/{1}",1,"drogon::GetFilter");//path will be /api/v1/ApiTest/get/{arg2}/{arg1}
        METHOD_ADD(ApiTest::your_method_name,"/{1}/{2}/list",0,"drogon::GetFilter");//path will be /api/v1/ApiTest/{arg1}/{arg2}/list
        
        METHOD_LIST_END
        //your declaration of processing function maybe like this:
        void get(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,std::string p2);
        void your_method_name(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,double p1,int p2) const;
        };
    }
}
