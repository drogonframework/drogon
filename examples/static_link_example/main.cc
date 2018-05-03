#include <drogon/HttpAppFramework.h>
int main()
{
    drogon::HttpAppFramework framework("0.0.0.0",12345);
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    framework.run();
}