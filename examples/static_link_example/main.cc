#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>

using namespace drogon;

int main()
{

    std::cout<<banner<<std::endl;
    drogon::HttpAppFramework::instance().addListener("0.0.0.0",12345);
    drogon::HttpAppFramework::instance().addListener("0.0.0.0",8080);
    trantor::Logger::setLogLevel(trantor::Logger::INFO);
    drogon::HttpAppFramework::instance().run();
}
