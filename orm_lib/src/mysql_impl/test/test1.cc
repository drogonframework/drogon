#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <unistd.h>
using namespace drogon::orm;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto clientPtr = DbClient::newMysqlClient("host= 127.0.0.1    port  =3306 dbname= test user = root  ", 1);
    sleep(1);
    *clientPtr << "show status" >> [](const Result &r) {} >> [](const DrogonDbException &e) {};
    getchar();
}