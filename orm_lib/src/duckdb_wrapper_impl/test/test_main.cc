#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/CoroMapper.h>
#include <iostream>
#include <memory>
#include <string_view>

using namespace drogon;
using namespace drogon::orm;

#ifdef __cpp_impl_coroutine
Task<> mainCoro(DbClientPtr client)
{
    try
    {
        // Test basic operations
        auto result = co_await client->execSqlCoro("CREATE TABLE IF NOT EXISTS test_table (id INTEGER, name VARCHAR)");
        std::cout << "Table created successfully (coro)" << std::endl;

        result = co_await client->execSqlCoro("INSERT INTO test_table VALUES (1, 'test1'), (2, 'test2')");
        std::cout << "Data inserted successfully (coro)" << std::endl;

        result = co_await client->execSqlCoro("SELECT * FROM test_table");
        std::cout << "Query executed successfully (coro)" << std::endl;
        std::cout << "Rows: " << result.size() << std::endl;
        for (auto const &row : result)
        {
            std::cout << "ID: " << row[0].as<int>()
                        << ", Name: " << row[1].as<std::string>() << std::endl;
        }
    }
    catch(const DrogonDbException &e)
    {
        std::cerr << "Failed to execute sql: " << e.base().what() << std::endl;
    }

    app().quit();
}
#endif

int main(int argc, char *argv[])
{
    app().setLogLevel(trantor::Logger::kDebug);
    
    // Create DuckDB client
    auto client = drogon::orm::DbClient::newDuckDbClient("filename=test.db", 1);
    
#ifdef __cpp_impl_coroutine
    if (argc > 1 && std::string_view(argv[1]) == "--coro")
    {
        std::cout << "Running coroutine test" << std::endl;
        app().getLoop()->queueInLoop([client]() { mainCoro(client); });
        app().run();
        return 0;
    }
#endif

    std::cout << "Running callback test" << std::endl;
    // Test basic operations
    client->execSqlAsync("CREATE TABLE IF NOT EXISTS test_table (id INTEGER, name VARCHAR)", 
                        [](const Result &result) {
                            std::cout << "Table created successfully" << std::endl;
                        },
                        [](const std::exception_ptr &e) {
                            std::cout << "Failed to create table" << std::endl;
                        });
    
    client->execSqlAsync("INSERT INTO test_table VALUES (1, 'test1'), (2, 'test2')",
                        [](const Result &result) {
                            std::cout << "Data inserted successfully" << std::endl;
                        },
                        [](const std::exception_ptr &e) {
                            std::cout << "Failed to insert data" << std::endl;
                        });
    
    client->execSqlAsync("SELECT * FROM test_table",
                        [](const Result &result) {
                            std::cout << "Query executed successfully" << std::endl;
                            std::cout << "Rows: " << result.size() << std::endl;
                            for (auto const &row : result)
                            {
                                std::cout << "ID: " << row[0].as<int>() 
                                         << ", Name: " << row[1].as<std::string>() << std::endl;
                            }
                            app().quit();
                        },
                        [](const std::exception_ptr &e) {
                            std::cout << "Failed to query data" << std::endl;
                            app().quit();
                        });
    
    // Test JSON column
    // client->execSqlAsync("CREATE TABLE IF NOT EXISTS test_json1 (id INTEGER, data JSON)",
    //                     [](const Result &result) {
    //                         std::cout << "JSON table created successfully" << std::endl;
    //                     },
    //                     [](const std::exception_ptr &e) {
    //                         std::cout << "Failed to create JSON table" << std::endl;
    //                     });

    // 使用 SqlBinder 显式绑定参数插入 JSON
    
    {
        auto binder = (*client) << "INSERT INTO test_json1 VALUES (?, ?)";
        binder << std::string("1") << std::string(R"({\"name\": \"Alice\", \"age\": 30})");
        binder >> [](const Result &result) {
            std::cout << "JSON data inserted successfully" << std::endl;
        };
        binder >> [](const std::exception_ptr &e) {
            std::cout << "Failed to insert JSON data" << std::endl;
        };
        binder.exec();
    }

    client->execSqlAsync("SELECT id, data FROM test_json1",
                        [](const Result &result) {
                            std::cout << "JSON select executed successfully" << std::endl;
                            for (auto const &row : result)
                            {
                                std::cout << "ID: " << row[0].as<int>()
                                          << ", JSON: " << row[1].as<std::string>() << std::endl;
                            }
                        },
                        [](const std::exception_ptr &e) {
                            std::cout << "Failed to select JSON data" << std::endl;
                        });
    
    app().run();
    return 0;
} 