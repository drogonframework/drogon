/**
 *
 *  @file db_listener_test.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 *  Drogon database test program
 *
 */

#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>
#include <drogon/orm/DbListener.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;
using namespace trantor;
using namespace std::chrono_literals;

#if USE_POSTGRESQL
orm::DbClientPtr postgreClient;

DROGON_TEST(ListenNotifyTest)
{
    auto clientPtr = postgreClient;
    auto dbListener = DbListener::newPgListener(clientPtr->connectionInfo());
    MANDATE(dbListener);

    std::vector<std::string> channels{"listen_test_0",
                                      "listen_test_1",
                                      "listen_test_2"};

    static int numNotifications = 0;
    LOG_INFO << "Start listen.";
    for (auto &chan : channels)
    {
        dbListener->listen(chan,
                           [TEST_CTX, chan](const std::string &channel,
                                            const std::string &message) {
                               MANDATE(channel == chan);
                               LOG_INFO << "Message from " << channel << ": "
                                        << message;
                               ++numNotifications;
                           });
    }

    std::this_thread::sleep_for(1s);  // ensure listen success
    LOG_INFO << "Start sending notifications.";
    for (int i = 0; i < 5; ++i)
    {
        for (auto &chan : channels)
        {
            // Can not use placeholders in LISTEN or NOTIFY command!!!
            std::string cmd =
                "NOTIFY " + chan + ", '" + std::to_string(i) + "'";
            clientPtr->execSqlAsync(
                cmd,
                [i, chan](const orm::Result &result) {
                    LOG_INFO << chan << " notified " << i;
                },
                [](const orm::DrogonDbException &ex) {
                    LOG_ERROR << "Failed to notify " << ex.base().what();
                });
        }
    }

    std::this_thread::sleep_for(5s);
    LOG_INFO << "Unlisten.";
    for (auto &chan : channels)
    {
        dbListener->unlisten(chan);
    }
    CHECK(numNotifications == 15);
    std::this_thread::sleep_for(1s);
}
#endif

int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);

    std::string dbConnInfo;
    const char *dbUrl = std::getenv("DROGON_TEST_DB_CONN_INFO");
    if (dbUrl)
    {
        dbConnInfo = std::string{dbUrl};
    }
    else
    {
        dbConnInfo =
            "host=127.0.0.1 port=5432 dbname=postgres user=postgres "
            "password=12345 "
            "client_encoding=utf8";
    }
    LOG_INFO << "Database conn info: " << dbConnInfo;
#if USE_POSTGRESQL
    postgreClient = orm::DbClient::newPgClient(dbConnInfo, 2, true);
#else
    LOG_DEBUG << "Drogon is built without Postgresql. No tests executed.";
    return 0;
#endif

    int testStatus = test::run(argc, argv);
    return testStatus;
}
