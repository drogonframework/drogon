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

class HHH:public drogon::DrObject<HHH>
{
public:
   // HHH(){std::cout<<"class HHH constr,name="<<className()<<std::endl;}
};

class TestController:public drogon::HttpSimpleController<TestController>
{
public:
    TestController(){}
    void ttt(){}
};





int main()
{
    //drogon::DrObjectBase *p=drogon::DrClassMap::newObject("HHH");
    //TestController haha;
    //std::cout<<haha.good()<<std::endl;
    HHH hh;
    std::cout<<banner<<std::endl;
    drogon::HttpAppFramework framework("0.0.0.0",12345);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    framework.run();
}
