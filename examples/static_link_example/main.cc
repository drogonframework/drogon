#include <drogon/HttpAppFramework.h>
#include <iostream>
static const char banner[]="     _                             \n"
                           "  __| |_ __ ___   __ _  ___  _ __  \n"
                           " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                           "| (_| | | | (_) | (_| | (_) | | | |\n"
                           " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                           "                 |___/             \n";

int main()
{
    std::cout<<banner<<std::endl;
    drogon::HttpAppFramework framework("0.0.0.0",12345);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    framework.run();
}
