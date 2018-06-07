#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpApiController.h>
#include <trantor/utils/Logger.h>
#include <vector>
#include <string>

using namespace drogon;
class A
{
public:
    void handle(const HttpRequest& req,
                const std::function<void (HttpResponse &)>&callback,
                int p1,const std::string &p2,const std::string &p3,int p4) const
    {
        HttpViewData data;
        data.insert("title",std::string("ApiTest::get"));
        std::map<std::string,std::string> para;
        para["int p1"]=std::to_string(p1);
        para["string p2"]=p2;
        para["string p3"]=p3;
        para["int p4"]=std::to_string(p4);

        data.insert("parameters",para);
        auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
        callback(*res);
    }
};
class B
{
public:
    void operator ()(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2)
    {
        HttpViewData data;
        data.insert("title",std::string("ApiTest::get"));
        std::map<std::string,std::string> para;
        para["p1"]=std::to_string(p1);
        para["p2"]=std::to_string(p2);
        data.insert("parameters",para);
        auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
        callback(*res);
    }
};
namespace api
{
    namespace v1
    {
        class Test:public HttpApiController<Test>
        {
            METHOD_LIST_BEGIN
            METHOD_ADD(Test::get,"/{2}/{1}",1,"drogon::GetFilter");//path will be /api/v1/test/get/{arg2}/{arg1}
            METHOD_ADD(Test::list,"/{2}/info",0,"drogon::GetFilter");//path will be /api/v1/test/{arg2}/info
            METHOD_LIST_END
            void get(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2) const
            {
                HttpViewData data;
                data.insert("title",std::string("ApiTest::get"));
                std::map<std::string,std::string> para;
                para["p1"]=std::to_string(p1);
                para["p2"]=std::to_string(p2);
                data.insert("parameters",para);
                auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
                callback(*res);
            }
            void list(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2) const
            {
                HttpViewData data;
                data.insert("title",std::string("ApiTest::get"));
                std::map<std::string,std::string> para;
                para["p1"]=std::to_string(p1);
                para["p2"]=std::to_string(p2);
                data.insert("parameters",para);
                auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
                callback(*res);
            }
        };
    }
}
using namespace std::placeholders;
int main()
{

    std::cout<<banner<<std::endl;

    drogon::HttpAppFramework::instance().addListener("0.0.0.0",12345);
    drogon::HttpAppFramework::instance().addListener("0.0.0.0",8080);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    //class function
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle1/{1}/{2}/{3}/{4}",&A::handle);
    //lambda example
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle2/{1}/{2}",[](const HttpRequest&req,const std::function<void (HttpResponse &)>&callback,int a,float b){
        LOG_DEBUG<<"int a="<<a;
        LOG_DEBUG<<"flaot b="<<b;
        HttpViewData data;
        data.insert("title",std::string("ApiTest::get"));
        std::map<std::string,std::string> para;
        para["a"]=std::to_string(a);
        para["b"]=std::to_string(b);
        data.insert("parameters",para);
        auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
        callback(*res);
    });

    B b;
    //functor example
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle3/{1}/{2}",b);

    A tmp;
    std::function<void(const HttpRequest&,const std::function<void (HttpResponse &)>&,int,const std::string &,const std::string &,int)>
            func=std::bind(&A::handle,&tmp,_1,_2,_3,_4,_5,_6);
    //api example for std::function
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle4/{4}/{3}/{1}",func);
            LOG_DEBUG<<drogon::DrObjectBase::demangle(typeid(func).name());
    drogon::HttpAppFramework::instance().run();

}
