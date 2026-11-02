#include <drogon/config.h>
#include <drogon/drogon_test.h>
#include <drogon/orm/ArrayParser.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <filesystem>
#include <string>

using namespace drogon::orm;

DROGON_TEST(ArrayParserStopsAtTerminator)
{
    // A truncated array: the bytes after the NUL must not be read.
    const char input[] = "{1,2\0,9}";
    ArrayParser parser(input);
    CHECK(parser.getNext().first == ArrayParser::juncture::row_start);
    auto value = parser.getNext();
    CHECK(value.first == ArrayParser::juncture::string_value);
    CHECK(value.second == "1");
    value = parser.getNext();
    CHECK(value.first == ArrayParser::juncture::string_value);
    CHECK(value.second == "2");
    CHECK(parser.getNext().first == ArrayParser::juncture::done);
}

#if USE_SQLITE3
DROGON_TEST(Sqlite3ParameterSafety)
{
    const auto dbPath =
        std::filesystem::current_path() / "orm_parameter_safety.db";
    std::filesystem::remove(dbPath);
    {
        auto client =
            DbClient::newSqlite3Client("filename=" + dbPath.string(), 1);
        client->execSqlSync("create table t(v text)");

        // Embedded NUL is stored, not truncated.
        client->execSqlSync("insert into t values(?)",
                            std::string("ab\0cd", 5));
        auto r = client->execSqlSync("select length(cast(v as blob)) from t");
        CHECK(r[0][0].as<int>() == 5);

        // Fewer arguments than placeholders on a cached statement is refused
        // rather than reusing the previous query's bindings.
        client->execSqlSync("select ?, ?", 1, std::string("x"));
        CHECK_THROWS_AS(client->execSqlSync("select ?, ?", 1), SqlError);
    }
    std::filesystem::remove(dbPath);
}
#endif

#if USE_POSTGRESQL
DROGON_TEST(PostgresRejectsNulInTextParameter)
{
    // The check runs before the query reaches the connection, so no server
    // is needed.
    auto client = DbClient::newPgClient(
        "host=127.0.0.1 port=1 dbname=none user=none connect_timeout=1", 1);
    CHECK_THROWS_AS(client->execSqlSync("select $1",
                                        std::string("admin\0x", 7)),
                    SqlError);
}
#endif
