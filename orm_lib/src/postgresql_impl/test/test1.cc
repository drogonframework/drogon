#include <drogon/orm/PgClient.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <unistd.h>
using namespace drogon::orm;

int main()
{
    drogon::orm::PgClient client("host=127.0.0.1 port=5432 dbname=trantor user=antao", 1);
    LOG_DEBUG << "start!";
    sleep(1);
    try
    {
        auto r = client.execSqlSync("select * from users where user_uuid=$1;", 1);
        for (auto row : r)
        {
            for (auto f : row)
            {
                std::cout << f.name() << " : " << f.as<int>() << std::endl;
            }
        }
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_DEBUG << "catch:" << e.base().what();
    }

    
    // client << "select count(*) from users" >> [](const drogon::orm::Result &r) {
    //     for (auto row : r)
    //     {
    //         for (auto f : row)
    //         {
    //             std::cout << f.name() << " : " << f.as<int>() << std::endl;
    //         }
    //     }
    // } >> [](const drogon::orm::DrogonDbException &e) {
    //     LOG_DEBUG << "except callback:" << e.base().what();
    // };

    // client << "select * from users limit 5" >> [](const drogon::orm::Result &r) {
    //     for (auto row : r)
    //     {
    //         for (auto f : row)
    //         {
    //             std::cout << f.name() << " : " << f.as<int>() << std::endl;
    //         }
    //     }
    // } >> [](const drogon::orm::DrogonDbException &e) {
    //     LOG_DEBUG << "except callback:" << e.base().what();
    // };

    // client << "select user_id,user_uuid from users where user_uuid=$1" 
    // << 2
    // >> [](bool isNull, const std::string &id, uint64_t uuid) {
    //     if (!isNull)
    //         std::cout << "id is " << id << "(" << uuid << ")" << std::endl;
    //     else
    //         std::cout << "no more!" << std::endl;
    // } >> [](const drogon::orm::DrogonDbException &e) {
    //     LOG_DEBUG << "except callback:" << e.base().what();
    // };
    // client.execSqlAsync("",
    //                     [](const drogon::orm::Result &r) {},
    //                     [](const drogon::orm::DrogonDbException &e) {
    //                         LOG_DEBUG << "async nonblocking except callback:" << e.base().what();
    //                     });
    // client.execSqlAsync("",
    //                     [](const drogon::orm::Result &r) {},
    //                     [](const drogon::orm::DrogonDbException &e) {
    //                         LOG_DEBUG << "async blocking except callback:" << e.base().what();
    //                     },
    //                     true);
    auto f = client.execSqlAsync("select * from users limit 5");
    try
    {
        auto r=f.get();
        for(auto row:r)
        {
            std::cout<<"user_id:"<<row["user_id"].as<std::string>()<<std::endl;
        }
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_DEBUG << "future exception:" << e.base().what();
    }
    // client << "\\d users"
    // >>[](const Result &r)
    // {
    //     std::cout<<"got a result!"<<std::endl;
    // }
    // >>[](const DrogonDbException &e)
    // {
    //     std::cout<< e.base().what()<<std::endl;
    // };
    getchar();
}