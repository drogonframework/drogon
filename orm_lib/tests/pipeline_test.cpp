#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>

using namespace drogon;
using namespace trantor;

#if LIBPQ_SUPPORTS_BATCH_MODE

static int step = 0;
static std::vector<std::function<void()>> testSteps;

static auto runNextStep = [] {
    LOG_INFO << "Step = " << step;
    if (step < testSteps.size())
    {
        app().getIOLoop(0)->runAfter(0.5, testSteps[step++]);
    }
    else
    {
        testSteps.clear();
    }
};

DROGON_TEST(PgPipelineTest)
{
    auto testStep_init = [TEST_CTX]() {
        auto clientPtr = app().getFastDbClient();
        clientPtr->execSqlAsync(
            "drop table if exists drogon_test_pipeline;",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Create table drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_init(0) what():" +
                      std::string(e.base().what()));
            });
        clientPtr->execSqlAsync(
            "drop function if exists fn_drogon_test_pipeline;",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Drop function fn_drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_init(1) what():" +
                      std::string(e.base().what()));
            });

        clientPtr->execSqlAsync(
            "create table drogon_test_pipeline"
            "("
            "   id      int     primary key,"
            "   name    text    unique"
            ");",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Create table drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_init(2) what():" +
                      std::string(e.base().what()));
            });

        clientPtr->execSqlAsync(
            "insert into"
            "   drogon_test_pipeline "
            "values"
            "   (1, 'trantor'),"
            "   (2, 'drogon');",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Insert data into drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_init(3) what():" +
                      std::string(e.base().what()));
            });

        clientPtr->execSqlAsync(
            "create function fn_drogon_test_pipeline(id int, name text) "
            "returns void "
            "language plpgsql "
            "as $$ "
            "begin "
            "   update drogon_test_pipeline t set name = $2 where t.id = $1;"
            "end $$;",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Insert data into drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_init(4) what():" +
                      std::string(e.base().what()));
            });

        runNextStep();
    };

    auto testStep_allSuccess = [TEST_CTX]() {
        auto clientPtr = app().getFastDbClient();
        clientPtr->execSqlAsync(
            "select 1",
            [TEST_CTX](const drogon::orm::Result &r) {
                MANDATE(r.size() == 1);
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_allSuccess(0) what():" +
                      std::string(e.base().what()));
            });
        clientPtr->execSqlAsync(
            "select * from drogon_test_pipeline",
            [TEST_CTX](const drogon::orm::Result &r) {
                MANDATE(r.size() == 2);
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_allSuccess(1) what():" +
                      std::string(e.base().what()));
            });

        runNextStep();
    };

    auto testStep_testAbort = [TEST_CTX]() {
        auto clientPtr = app().getFastDbClient();
        clientPtr->execSqlAsync(
            "select name from drogon_test_pipeline where id = $1",
            [TEST_CTX](const drogon::orm::Result &r) {
                MANDATE(r.size() == 1);
                MANDATE(r[0][0].as<std::string>() == "trantor");
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_testAbort(0) what():" +
                      std::string(e.base().what()));
            },
            1);

        // Update name
        // NOTICE: update in a function to bypass auto sync
        // DO NOT USE like this in production
        clientPtr->execSqlAsync(
            "select * from fn_drogon_test_pipeline($1, $2)",
            [TEST_CTX](const drogon::orm::Result &r) { SUCCESS(); },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_testAbort(1) what():" +
                      std::string(e.base().what()));
            },
            1,
            "terminus");

        // Check name
        clientPtr->execSqlAsync(
            "select name from drogon_test_pipeline where id = $1",
            [TEST_CTX](const drogon::orm::Result &r) {
                MANDATE(r.size() == 1);
                MANDATE(r[0][0].as<std::string>() == "terminus");
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_testAbort(2) what():" +
                      std::string(e.base().what()));
            },
            1);

        // Cause an error
        clientPtr->execSqlAsync(
            "select 'abc'::int",
            [TEST_CTX](const drogon::orm::Result &r) {
                FAULT("PgPipelineTest_testAbort(3) should not pass");
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) { SUCCESS(); },
            1);

        clientPtr->execSqlAsync(
            "select 1",
            [TEST_CTX](const drogon::orm::Result &r) {
                FAULT("PgPipelineTest_testAbort(3) should not pass");
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) { SUCCESS(); },
            1);

        runNextStep();
    };

    auto testStep_afterAbort = [TEST_CTX]() {
        auto clientPtr = app().getFastDbClient();
        // Should remain unchanged, because last sync point was rollback.
        clientPtr->execSqlAsync(
            "select name from drogon_test_pipeline where id = $1",
            [TEST_CTX](const drogon::orm::Result &r) {
                MANDATE(r.size() == 1);
                MANDATE(r[0][0].as<std::string>() == "trantor");
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_testAbort(0) what():" +
                      std::string(e.base().what()));
            },
            1);

        runNextStep();
    };

    auto testStep_cleanup = [TEST_CTX]() {
        auto clientPtr = app().getFastDbClient();
        clientPtr->execSqlAsync(
            "drop table drogon_test_pipeline;",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Drop table drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_cleanup(0) what():" +
                      std::string(e.base().what()));
            });

        clientPtr->execSqlAsync(
            "drop function fn_drogon_test_pipeline;",
            [TEST_CTX](const drogon::orm::Result &r) {
                LOG_INFO << "Drop function fn_drogon_test_pipeline.";
            },
            [TEST_CTX](const drogon::orm::DrogonDbException &e) {
                FAULT("PgPipelineTest_cleanup(1) what():" +
                      std::string(e.base().what()));
            });
        runNextStep();
    };

    testSteps = {std::move(testStep_init),
                 std::move(testStep_allSuccess),
                 std::move(testStep_testAbort),
                 std::move(testStep_afterAbort),
                 std::move(testStep_cleanup)};

    runNextStep();
}

#endif

int main(int argc, char **argv)
{
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();
    app().setThreadNum(1);
#if LIBPQ_SUPPORTS_BATCH_MODE
    app().createDbClient(  //
        "postgresql",      // dbType
        "127.0.0.1",       // host
        5432,              // port
        "postgres",        // dbname
        "postgres",        // username
        "12345",           // password
        1,                 // connectionNum
        "",                // filename
        "default",         // name
        true,              // isFast
        "",                // charset
        10,                 // timeout
        true               // autobatch
    );
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
