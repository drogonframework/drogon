#include <drogon/orm/DbClient.h>
#include <iostream>
#include <trantor/utils/Logger.h>
#include <unistd.h>
using namespace drogon::orm;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto clientPtr = DbClient::newPgClient("host=127.0.0.1 port=5432 dbname=test user=antao", 3);
    LOG_DEBUG << "start!";
    sleep(1);
    *clientPtr << "update group_users set join_date=$1,relationship=$2 where "
                  "g_uuid=420040 and u_uuid=2"
               << nullptr << nullptr << Mode::Blocking >>
        [](const Result &r) { std::cout << "update " << r.affectedRows() << " lines" << std::endl; } >>
        [](const DrogonDbException &e) { std::cerr << e.base().what() << std::endl; };
    try
    {
        auto r = clientPtr->execSqlSync("select * from users where user_uuid=$1;", 1);
        for (auto const &row : r)
        {
            for (auto const &f : row)
            {
                std::cout << f.name() << " : " << f.as<int>() << std::endl;
            }
        }
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_DEBUG << "catch:" << e.base().what();
    }

    // client << "select count(*) from users" >> [](const drogon::orm::Result &r)
    // {
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

    // client << "select * from users limit 5" >> [](const drogon::orm::Result &r)
    // {
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
    //                         LOG_DEBUG << "async blocking except callback:" <<
    //                         e.base().what();
    //                     },
    //                     true);
    auto f =
        clientPtr->execSqlAsyncFuture("select * from users where user_uuid > $1 limit $2 offset $3", 100, (size_t)2, (size_t)2);
    try
    {
        auto r = f.get();
        for (auto const &row : r)
        {
            std::cout << "user_id:" << row["user_id"].as<std::string>() << std::endl;
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
    clientPtr->execSqlAsync(
        "select * from users where user_uuid=$1;",
        [](const drogon::orm::Result &r) { LOG_DEBUG << "row count:" << r.size(); },
        [](const drogon::orm::DrogonDbException &e) { LOG_DEBUG << "async nonblocking except callback:" << e.base().what(); },
        1);
    clientPtr->execSqlAsync("select * from users where org_name=$1",
                            [](const Result &r) {
                                std::cout << r.size() << " rows selected!" << std::endl;
                                int i = 0;
                                for (auto row : r)
                                {
                                    std::cout << i++ << ": user name is " << row["user_name"].as<std::string>() << std::endl;
                                }
                            },
                            [](const DrogonDbException &e) { std::cerr << "error:" << e.base().what() << std::endl; },
                            "default");
    *clientPtr << "select t2,t9 from ttt where t7=2" >> [](const Result &r) {
        std::cout << r.size() << " rows selected!" << std::endl;
        for (auto row : r)
        {
            std::cout << row["t9"].as<std::string>() << std::endl;
            std::cout << row["t2"].as<int>() << std::endl;
        }
    } >> [](const DrogonDbException &e) { std::cerr << "error:" << e.base().what() << std::endl; };
    getchar();
}
