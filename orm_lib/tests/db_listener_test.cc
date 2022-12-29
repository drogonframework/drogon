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

static const std::string LISTEN_CHANNEL = "listen_test";

#if USE_POSTGRESQL
orm::DbClientPtr postgreClient;
DROGON_TEST(ListenNotifyTest)
{
    auto clientPtr = postgreClient;
    auto dbListener = DbListener::newPgListener(clientPtr->connectionInfo());
    MANDATE(dbListener);

    int numNotifications = 0;
    dbListener->listen(LISTEN_CHANNEL,
                       [TEST_CTX,
                        &numNotifications](const std::string &channel,
                                           const std::string &message) {
                           MANDATE(channel == LISTEN_CHANNEL);
                           LOG_INFO << "Message from " << LISTEN_CHANNEL << ": "
                                    << message;
                           ++numNotifications;
                       });

    std::thread t([TEST_CTX, clientPtr, dbListener]() {
        std::this_thread::sleep_for(1s);
        for (int i = 0; i < 10; ++i)
        {
            // Can not use placeholders in LISTEN or NOTIFY command!!!
            std::string cmd =
                "NOTIFY " + LISTEN_CHANNEL + ", '" + std::to_string(i) + "'";
            clientPtr->execSqlAsync(
                cmd,
                [i](const orm::Result &result) {
                    LOG_INFO << "Notified " << i;
                },
                [](const orm::DrogonDbException &ex) {
                    LOG_ERROR << "Failed to notify " << ex.base().what();
                });
        }
        std::this_thread::sleep_for(5s);
        dbListener->unlisten("listen_test");
    });

    t.join();
    CHECK(numNotifications == 10);
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
#endif

    int testStatus = test::run(argc, argv);
    return testStatus;
}
