#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <unistd.h>
using namespace drogon::orm;
using namespace drogon;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto clientPtr = DbClient::newMysqlClient("host= 127.0.0.1    port  =3306 dbname= test user = root  ", 1);
    sleep(1);
    for (int i = 0; i < 10; i++)
    {
        std::string str = formattedString("insert into users (user_id,user_name,org_name) values('%d','antao','default')", i);
        *clientPtr << str >> [](const Result &r) {
            std::cout << "insert rows:" << r.affectedRows() << std::endl;
        } >> [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    }

    *clientPtr << "select * from users" >> [](const Result &r) {
        std::cout << "rows:" << r.size() << std::endl;
        std::cout << "column num:" << r.columns() << std::endl;
        // for (auto row : r)
        // {
        //     std::cout << "user_id=" << row["user_id"].as<std::string>() << std::endl;
        // }
        // for (auto row : r)
        // {
        //     for (auto f : row)
        //     {
        //         std::cout << f.name() << ":" << (f.isNull() ? "NULL" : f.as<std::string>()) << std::endl;
        //     }
        // }
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
    };

    *clientPtr << "select * from users where id!=? order by id"
               << 139 >>
        [](const Result &r) {
            std::cout << "rows:" << r.size() << std::endl;
            std::cout << "column num:" << r.columns() << std::endl;
            // for (auto row : r)
            // {
            //     std::cout << "user_id=" << row["user_id"].as<std::string>() << " id=" << row["id"].as<int>() << std::endl;
            // }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    getchar();
}