#include <drogon/DrObject.h>
#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <drogon/HttpSimpleController.h>
static const char banner[]="     _                             \n"
                           "  __| |_ __ ___   __ _  ___  _ __  \n"
                           " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                           "| (_| | | | (_) | (_| | (_) | | | |\n"
                           " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                           "                 |___/             \n";
using namespace drogon;
class HHH:public drogon::DrObject<HHH>
{
public:
   // HHH(){std::cout<<"class HHH constr,name="<<className()<<std::endl;}
};

class TestController:public drogon::HttpSimpleController<TestController>
{
public:
    TestController(){}
    virtual void asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)override
    {
        HttpResponse resp;
        LOG_DEBUG<<"!!!!!!!!!!1";
        resp.setStatusCode(HttpResponse::k200Ok);

        resp.setContentTypeCode(CT_TEXT_HTML);

        resp.setBody("hello!!!");
        callback(resp);
    }
    void ttt(){}
    PATH_LIST_BEGIN
    PATH_ADD("/");
    PATH_ADD("/Test");
    PATH_ADD("/tpost","post","login");
    PATH_LIST_END
};





int main()
{
    //drogon::DrObjectBase *p=drogon::DrClassMap::newObject("HHH");
    //TestController haha;
    //std::cout<<haha.good()<<std::endl;

    std::cout<<banner<<std::endl;
    drogon::HttpAppFramework::instance().setListening("0.0.0.0",12345);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    drogon::HttpAppFramework::instance().run();
}
