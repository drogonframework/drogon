#include "Groups.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <iostream>
#include <trantor/utils/Logger.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace drogon::orm;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    auto clientPtr = DbClient::newSqlite3Client("filename=test.db", 1);
    std::this_thread::sleep_for(1s);

    LOG_DEBUG << "start!";
    // *clientPtr << "Drop table groups;" << Mode::Blocking >>
    //     [](const Result &r) {
    //         LOG_DEBUG << "droped";
    //     } >>
    //     [](const DrogonDbException &e) {
    //         std::cout << e.base().what() << std::endl;
    //     };
    // ;
    *clientPtr
            << "CREATE TABLE IF NOT EXISTS GROUPS (GROUP_ID INTEGER PRIMARY KEY autoincrement,\
     GROUP_NAME TEXT,\
     CREATER_ID INTEGER,\
     CREATE_TIME TEXT,\
     INVITING INTEGER,\
     INVITING_USER_ID INTEGER,\
     AVATAR_ID TEXT, uuu double, text VARCHAR(255),avatar blob,is_default bool)"
            << Mode::Blocking >>
        [](const Result &r) { LOG_DEBUG << "created"; } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "insert into GROUPS (group_name) values(?)"
               << "test_group" << Mode::Blocking >>
        [](const Result &r) {
            LOG_DEBUG << "inserted:" << r.affectedRows();
            LOG_DEBUG << "id:" << r.insertId();
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "insert into GROUPS (group_name) values(?)"
               << "test_group" << Mode::Blocking >>
        [](const Result &r) {
            LOG_DEBUG << "inserted:" << r.affectedRows();
            LOG_DEBUG << "id:" << r.insertId();
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "select * from GROUPS " >> [](const Result &r) {
        LOG_DEBUG << "affected rows:" << r.affectedRows();
        LOG_DEBUG << "select " << r.size() << " rows";
        LOG_DEBUG << "id:" << r.insertId();
        for (auto const &row : r)
        {
            LOG_DEBUG << "group_id:" << row["group_id"].as<size_t>();
        }
    } >> [](const DrogonDbException &e) {
        std::cout << e.base().what() << std::endl;
    };
    {
        auto trans = clientPtr->newTransaction([](bool success) {
            LOG_DEBUG << (success ? "commit success!" : "commit failed!");
        });
        Mapper<drogon_model::sqlite3::Groups> mapper(trans);
        mapper.limit(2).offset(1).findAll(
            [trans](const std::vector<drogon_model::sqlite3::Groups> &v) {
                Mapper<drogon_model::sqlite3::Groups> mapper(trans);
                for (auto group : v)
                {
                    LOG_DEBUG << "group_id=" << group.getValueOfGroupId();
                    std::cout << group.toJson() << std::endl;
                    std::cout << "avatar:" << group.getValueOfAvatarAsString()
                              << std::endl;
                    group.setAvatarId("xixi");
                    mapper.update(
                        group,
                        [](const size_t count) {
                            LOG_DEBUG << "update " << count << " rows";
                        },
                        [](const DrogonDbException &e) {
                            LOG_ERROR << e.base().what();
                        });
                }
            },
            [](const DrogonDbException &e) { LOG_ERROR << e.base().what(); });
        drogon_model::sqlite3::Groups group;
        group.setAvatar("hahahaha,xixixixix");
        try
        {
            mapper.insert(group);
        }
        catch (const DrogonDbException &e)
        {
            std::cerr << e.base().what() << std::endl;
        }
        *clientPtr << "select is_default from groups" >> [](const Result &r) {
            for (auto row : r)
            {
                std::cout << "is_default: "
                          << (row[(unsigned long)0].isNull() ? "is null, "
                                                             : "is not null, ")
                          << "bool value:" << row[(unsigned long)0].as<bool>()
                          << "(" << row[(unsigned long)0].as<std::string>()
                          << ")" << std::endl;
            }
        } >> [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    }
    getchar();
}
