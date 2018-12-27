#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <unistd.h>
using namespace drogon::orm;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto clientPtr = DbClient::newSqlite3Client("filename=test.db", 3);

    sleep(1);
    LOG_DEBUG << "start!";
    // *clientPtr << "Drop table groups;" << Mode::Blocking >>
    //     [](const Result &r) {
    //         LOG_DEBUG << "droped";
    //     } >>
    //     [](const DrogonDbException &e) {
    //         std::cout << e.base().what() << std::endl;
    //     };
    // ;
    *clientPtr << "CREATE TABLE IF NOT EXISTS GROUPS (GROUP_ID INTEGER PRIMARY KEY autoincrement,\
     GROUP_NAME TEXT,\
     CREATER_ID INTEGER,\
     CREATE_TIME TEXT,\
     INVITING INTEGER,\
     INVITING_USER_ID INTEGER,\
     AVATAR_ID TEXT)"
               << Mode::Blocking >>
        [](const Result &r) {
            LOG_DEBUG << "created";
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
    *clientPtr << "select * from GROUPS " >>
        [](const Result &r) {
            LOG_DEBUG << "affected rows:" << r.affectedRows();
            LOG_DEBUG << "select " << r.size() << " rows";
            LOG_DEBUG << "id:" << r.insertId();
            for(auto row:r)
            {
                LOG_DEBUG << "group_id:" << row["group_id"].as<size_t>();
            }
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    getchar();
}