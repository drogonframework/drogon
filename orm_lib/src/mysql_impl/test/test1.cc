#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace drogon::orm;
using namespace drogon;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    auto clientPtr = DbClient::newMysqlClient(
        "host= 127.0.0.1    port  =3306 dbname= test user = root  ", 1);
    std::this_thread::sleep_for(1s);
    // for (int i = 0; i < 10; ++i)
    // {
    //     std::string str = formattedString("insert into users
    //     (user_id,user_name,org_name) values('%d','antao','default')",
    //     i);
    //     *clientPtr << str >> [](const Result &r) {
    //         std::cout << "insert rows:" << r.affectedRows() << std::endl;
    //     } >> [](const DrogonDbException &e) {
    //         std::cerr << e.base().what() << std::endl;
    //     };
    // }
    LOG_TRACE << "begin";
    *clientPtr << "select * from users where id!=139 order by id"
               << Mode::Blocking >>
        [](const Result &r) {
            std::cout << "rows:" << r.size() << std::endl;
            std::cout << "column num:" << r.columns() << std::endl;

            for (auto const &row : r)
            {
                std::cout << "id=" << row["id"].as<int>() << std::endl;
                std::cout << "id=" << row["id"].as<std::string>() << std::endl;
            }
            // for (auto row : r)
            // {
            //     for (auto f : row)
            //     {
            //         std::cout << f.name() << ":" << (f.isNull() ? "NULL" :
            //         f.as<std::string>()) << std::endl;
            //     }
            // }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    LOG_TRACE << "end";
    LOG_TRACE << "begin";
    *clientPtr << "select * from users where id=? and user_id=? order by id"
               << 139 << "233" << Mode::Blocking >>
        [](const Result &r) {
            std::cout << "rows:" << r.size() << std::endl;
            std::cout << "column num:" << r.columns() << std::endl;
            for (auto const &row : r)
            {
                std::cout << "user_id=" << row["user_id"].as<std::string>()
                          << " id=" << row["id"].as<std::string>();
                std::cout << " time=" << row["time"].as<std::string>()
                          << std::endl;
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    *clientPtr << "update users set time=? where id>?" << trantor::Date::date()
               << 1000 << Mode::Blocking >>
        [](const Result &r) {
            std::cout << "update " << r.affectedRows() << " rows" << std::endl;
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };

    // std::ifstream infile("Makefile", std::ifstream::binary);
    // std::streambuf *pbuf = infile.rdbuf();
    // std::streamsize filesize = pbuf->pubseekoff(0, infile.end);
    // pbuf->pubseekoff(0, infile.beg); // rewind
    // std::string str;
    // str.resize(filesize);
    // pbuf->sgetn(&str[0], filesize);

    {
        auto trans = clientPtr->newTransaction([](bool ret) {
            if (ret)
            {
                std::cout << "committed!!!!!!" << std::endl;
            }
        });
        *trans << "update users set file=? where id != ?"
               << "hehaha" << 1000 >>
            [](const Result &r) {
                std::cout << "hahaha update " << r.affectedRows() << " rows"
                          << std::endl;
                // trans->rollback();
            } >>
            [](const DrogonDbException &e) {
                std::cerr << e.base().what() << std::endl;
            };
    }
    LOG_DEBUG << "out of transaction block";
    *clientPtr << "select * from users where id=1000" >> [](const Result &r) {
        std::cout << "file:" << r[0]["file"].as<std::string>() << std::endl;
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
    };

    *clientPtr << "select * from users limit ? offset ?" << 2 << 2 >>
        [](const Result &r) {
            std::cout << "select " << r.size() << " records" << std::endl;
            for (auto row : r)
            {
                std::cout << "is admin:" << row["admin"].as<bool>() << "("
                          << row["admin"].as<std::string>() << ")" << std::endl;
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    LOG_TRACE << "end";
    getchar();
}
