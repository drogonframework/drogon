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






int main()
{

    std::cout<<banner<<std::endl;
    drogon::HttpAppFramework::instance().setListening("0.0.0.0",12345);
    trantor::Logger::setLogLevel(trantor::Logger::INFO);
    drogon::HttpAppFramework::instance().run();
}
