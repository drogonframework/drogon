#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <drogon/HttpApiBinder.h>
#include <vector>
#include <string>
using namespace drogon;
class A
{
public:
    void handle(const HttpRequest& req,std::function<void (HttpResponse &)>callback,int aa,const std::string &a,const std::string &b,int haha)
    {
        LOG_DEBUG<<"int aa="<<aa;
        LOG_DEBUG<<"string a="<<a;
        LOG_DEBUG<<"string b="<<b;
        LOG_DEBUG<<"int haha="<<haha;
    }
};
int main()
{

    std::cout<<banner<<std::endl;
    auto bindPtr=std::make_shared<drogon::HttpApiBinder<decltype(&A::handle)>>(&A::handle);
    //drogon::HttpApiBinder<A, decltype(&A::handle)> binder(&A::handle);
   // binder.test();

    drogon::HttpAppFramework::instance().addListener("0.0.0.0",12345);
    drogon::HttpAppFramework::instance().addListener("0.0.0.0",8080);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    drogon::HttpAppFramework::instance().registerHttpApiController("/api/v1","",bindPtr);
    drogon::HttpAppFramework::instance().run();

}
