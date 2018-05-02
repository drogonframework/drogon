#include <drogon/HttpAppFramework.h>
int main()
{
    drogon::HttpAppFramework framework("0.0.0.0",12345);
    framework.run();
}