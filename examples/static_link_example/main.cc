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
                int aa,const std::string &a,const std::string &b,int haha) const
    {
        LOG_DEBUG<<"int aa="<<aa;
        LOG_DEBUG<<"string a="<<a;
        LOG_DEBUG<<"string b="<<b;
        LOG_DEBUG<<"int haha="<<haha;
    }
};
class B
{
public:
    void operator ()(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2)
    {
        LOG_DEBUG<<"int p1="<<p1;
        LOG_DEBUG<<"int p2="<<p2;
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
            METHOD_ADD(Test::list,"/{2}/list",0,"drogon::GetFilter");//path will be /api/v1/test/{arg2}/list
            METHOD_LIST_END
            void get(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2) const
            {
                LOG_DEBUG<<"int p1="<<p1;
                LOG_DEBUG<<"int p2="<<p2;
            }
            void list(const HttpRequest& req,const std::function<void (HttpResponse &)>&callback,int p1,int p2) const
            {
                LOG_DEBUG<<"int p1="<<p1;
                LOG_DEBUG<<"int p2="<<p2;
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
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle1","/{1}/{2}/{3}/{4}",&A::handle);
    //lambda
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle2","",[](const HttpRequest&req,const std::function<void (HttpResponse &)>&callback,int a,float b){
        LOG_DEBUG<<"int a="<<a;
        LOG_DEBUG<<"flaot b="<<b;
    });

    B b;
    //functor
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle3","",b);

    A tmp;
    std::function<void(const HttpRequest&,const std::function<void (HttpResponse &)>&,int,const std::string &,const std::string &,int)>
            func=std::bind(&A::handle,&tmp,_1,_2,_3,_4,_5,_6);
    //std::function
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle4","",func);
            LOG_DEBUG<<drogon::DrObjectBase::demangle(typeid(func).name());
    drogon::HttpAppFramework::instance().run();

}
