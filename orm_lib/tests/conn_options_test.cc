#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>

using namespace drogon;
using namespace trantor;

#if USE_POSTGRESQL
DROGON_TEST(ConnOptionsTest)
{
    auto clientPtr = app().getDbClient();

    // Run a query that will take longer than the statement_timeout
    clientPtr->execSqlAsync(
        "select pg_sleep(5);",
        [TEST_CTX](const drogon::orm::Result &r) {
            LOG_INFO << "select pg_sleep(5);";
            FAULT("Statement should be canceled due to timeout.");
        },
        [TEST_CTX](const drogon::orm::DrogonDbException &e) {
            LOG_INFO << "select pg_sleep(5); error(expected):"
                     << e.base().what();
            SUCCESS();
        });

    // Run sql codes to hold a lock for sometime
    clientPtr->execSqlAsync(
        R"(
DO $$
BEGIN
    perform pg_advisory_xact_lock(12345);
    perform pg_sleep(2);
END $$;)",
        [TEST_CTX](const drogon::orm::Result &r) {
            LOG_INFO << "pg_advisory_xact_lock transaction1 finished;";
            SUCCESS();
        },
        [TEST_CTX](const drogon::orm::DrogonDbException &e) {
            FAULT("query failed: ", e.base().what());
        });

    // Run sql codes that requires the same lock, and will be canceled due to
    // lock_timeout
    clientPtr->execSqlAsync(
        R"(
DO $$
BEGIN
    perform pg_sleep(0.5);
    perform pg_advisory_xact_lock(12345);
END $$;)",
        [TEST_CTX](const drogon::orm::Result &r) {
            LOG_INFO << "pg_advisory_xact_lock transaction2 finished;";
            FAULT("Statement should be canceled due to timeout.");
        },
        [TEST_CTX](const drogon::orm::DrogonDbException &e) {
            LOG_INFO << "pg_advisory_xact_lock() error(expected):"
                     << e.base().what();
            SUCCESS();
        });
}

#endif

int main(int argc, char **argv)
{
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();
    app().setThreadNum(1);
#if USE_POSTGRESQL
    app().addDbClient(orm::PostgresConfig{
        "127.0.0.1",  // host
        5432,         // port
        "postgres",   // dbname
        "postgres",   // username
        "12345",      // password
        4,            // connectionNum
        "default",    // name
        false,        // isFast
        "",           // charset
        10,           // timeout
        false,        // autobatch
        {
            {"statement_timeout", "3s"},
            {"lock_timeout", "0.5s"},
        }  // connectOptions
    });
#endif
    std::thread thr([&]() {
        app().getLoop()->queueInLoop([&]() { p1.set_value(); });
        app().run();
    });

    f1.get();
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
