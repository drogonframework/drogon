#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>
#include <drogon/orm/DbSubscriber.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;
using namespace trantor;
using namespace std::chrono_literals;

static const std::string LISTEN_CHANNEL = "listen_test";

orm::DbClientPtr postgreClient;

DROGON_TEST(ListenNotifyTest)
{
    auto clientPtr = postgreClient;
    auto subscriber = clientPtr->newSubscriber();

    int numNotifications = 0;
    subscriber->subscribe(LISTEN_CHANNEL,
                          [TEST_CTX,
                           &numNotifications](const std::string &channel,
                                              const std::string &message) {
                              MANDATE(channel == LISTEN_CHANNEL);
                              LOG_INFO << "Message from " << LISTEN_CHANNEL
                                       << ": " << message;
                              ++numNotifications;
                          });

    std::thread t([TEST_CTX, clientPtr, subscriber]() {
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
        subscriber->unsubscribe("listen_test");
    });

    t.join();
    CHECK(numNotifications == 10);
}

int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);

#if USE_POSTGRESQL
    postgreClient = orm::DbClient::newPgClient(
        "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=12345"
        "client_encoding=utf8",
        2,
        true);
#endif

    int testStatus = test::run(argc, argv);
    return testStatus;
}
