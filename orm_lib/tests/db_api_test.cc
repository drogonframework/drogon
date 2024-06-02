#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>

using namespace drogon;
using namespace trantor;

DROGON_TEST(DbApiTest)
{
#if USE_POSTGRESQL
    {
        auto client = app().getDbClient("pg_non_fast");
        CHECK(client != nullptr);
        client->closeAll();
        drogon::app().getLoop()->runInLoop([TEST_CTX]() {
            auto client = app().getFastDbClient("pg_fast");
            CHECK(client != nullptr);
            client->closeAll();
        });
        drogon::app().getIOLoop(0)->runInLoop([TEST_CTX]() {
            auto client = app().getFastDbClient("pg_fast");
            CHECK(client != nullptr);
            client->closeAll();
        });
    }
#endif

#if USE_MYSQL
    {
        auto client = app().getDbClient("mysql_non_fast");
        CHECK(client != nullptr);
        client->closeAll();
        drogon::app().getLoop()->runInLoop([TEST_CTX]() {
            auto client = app().getFastDbClient("mysql_fast");
            CHECK(client != nullptr);
            client->closeAll();
        });
        drogon::app().getIOLoop(0)->runInLoop([TEST_CTX]() {
            auto client = app().getFastDbClient("mysql_fast");
            CHECK(client != nullptr);
            client->closeAll();
        });
    }
#endif

#if USE_SQLITE3
    {
        auto client = app().getDbClient("sqlite3_non_fast");
        CHECK(client != nullptr);
        client->closeAll();
    }
#endif

    app().getLoop()->runAfter(5, [TEST_CTX]() {});  // wait for some time
}

const std::string_view pg_non_fast_config = R"({
    "name": "pg_non_fast",
    "rdbms": "postgresql",
    "host": "127.0.0.1",
    "port": 5432,
    "dbname": "test",
    "user": "postgres",
    "passwd": "123456",
    "is_fast": false
})";

const std::string_view pg_fast_config = R"({
    "name": "pg_fast",
    "rdbms": "postgresql",
    "host": "127.0.0.1",
    "port": 5432,
    "dbname": "test",
    "user": "postgres",
    "passwd": "123456",
    "is_fast": true
})";

const std::string_view mysql_non_fast_config = R"({
    "name": "mysql_non_fast",
    "rdbms": "mysql",
    "host": "127.0.0.1",
    "port": 3306,
    "dbname": "test",
    "user": "root",
    "passwd": "123456",
    "is_fast": false
})";

const std::string_view mysql_fast_config = R"({
    "name": "mysql_fast",
    "rdbms": "mysql",
    "host": "127.0.0.1",
    "port": 3306,
    "dbname": "test",
    "user": "root",
    "passwd": "123456",
    "is_fast": true
})";

const std::string_view sqlite3_non_fast_config = R"({
    "name": "sqlite3_non_fast",
    "rdbms": "sqlite3",
    "filename": "test.db",
    "is_fast": false
})";

bool parseJson(std::string_view str, Json::Value *root, Json::String *errs)
{
    static Json::CharReaderBuilder &builder =
        []() -> Json::CharReaderBuilder & {
        static Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        return builder;
    }();
    std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
    return jsonReader->parse(str.data(), str.data() + str.size(), root, errs);
}

int main(int argc, char **argv)
{
    Json::Value config;
    config["app"]["number_of_threads"] = 1;
    config["db_clients"] = Json::arrayValue;

    std::string err;
    Json::Value cfg;
#if USE_POSTGRESQL
    cfg = Json::objectValue;
    if (!parseJson(pg_non_fast_config, &cfg, &err))
    {
        LOG_ERROR << "Failed to parse json: " << err;
        return 1;
    }
    config["db_clients"].append(cfg);
    cfg = Json::objectValue;
    if (!parseJson(pg_fast_config.data(), &cfg, &err))
    {
        LOG_ERROR << "Failed to parse json: " << err;
        return 1;
    }
    config["db_clients"].append(cfg);
#endif

#if USE_MYSQL
    cfg = Json::objectValue;
    if (!parseJson(mysql_non_fast_config, &cfg, &err))
    {
        LOG_ERROR << "Failed to parse json: " << err;
        return 1;
    }
    config["db_clients"].append(cfg);
    cfg = Json::objectValue;
    if (!parseJson(mysql_fast_config.data(), &cfg, &err))
    {
        LOG_ERROR << "Failed to parse json: " << err;
        return 1;
    }
    config["db_clients"].append(cfg);
#endif

#if USE_SQLITE3
    cfg = Json::objectValue;
    if (!parseJson(sqlite3_non_fast_config, &cfg, &err))
    {
        LOG_ERROR << "Failed to parse json: " << err;
        return 1;
    }
    config["db_clients"].append(cfg);
#endif

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();
    app().setThreadNum(1);
    std::thread thr([&]() {
        app().getLoop()->queueInLoop([&]() { p1.set_value(); });
        app().loadConfigJson(config).run();
    });

    f1.get();
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
