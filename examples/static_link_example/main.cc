#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
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
using namespace std::placeholders;
int main()
{

    std::cout<<banner<<std::endl;

    auto bindPtr=std::make_shared<drogon::HttpApiBinder<decltype(&A::handle)>>(&A::handle);

    drogon::HttpAppFramework::instance().addListener("0.0.0.0",12345);
    drogon::HttpAppFramework::instance().addListener("0.0.0.0",8080);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle1","",&A::handle);

    drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle2","",[](const HttpRequest&req,std::function<void (HttpResponse &)>callback,int a,float b){
        LOG_DEBUG<<"int a="<<a;
        LOG_DEBUG<<"flaot b="<<b;
    });
   // A tmp;
  //  std::function<void(const HttpRequest&,std::function<void (HttpResponse &)>,int,const std::string &,const std::string &,int)>
  //           func();
  //  drogon::HttpAppFramework::registerHttpApiMethod("/api/v1/handle3","",std::bind(&A::handle,&tmp,_1,_2,_3,_4,_5,_6));
    drogon::HttpAppFramework::instance().run();

}
