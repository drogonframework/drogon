#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/SqlBinder.h>
#include <iostream>
#include <memory>
#include <string_view>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>

using namespace drogon;
using namespace drogon::orm;

// 测试计数器
std::atomic<int> testCounter{0};
std::atomic<int> successCounter{0};
std::atomic<int> failureCounter{0};

// 测试辅助函数
void printTestResult(const std::string &testName, bool success)
{
    testCounter++;
    if (success)
    {
        successCounter++;
        LOG_INFO << "✓ " << testName << " - PASSED";
    }
    else
    {
        failureCounter++;
        LOG_INFO << "✗ " << testName << " - FAILED";
    }
}

void printTestSummary()
{
    LOG_INFO << "\n=== 测试总结 ===";
    LOG_INFO << "总测试数: " << testCounter.load();
    LOG_INFO << "成功: " << successCounter.load();
    LOG_INFO << "失败: " << failureCounter.load();
    LOG_INFO << "成功率: "
             << (testCounter.load() > 0
                     ? (successCounter.load() * 100.0 / testCounter.load())
                     : 0)
             << "%";
}

#ifdef __cpp_impl_coroutine
Task<> comprehensiveCoroTest(DbClientPtr client)
{
    try
    {
        LOG_INFO << "\n=== 协程模式测试 ===";

        // 1. 基本表操作
        auto result = co_await client->execSqlCoro(
            "CREATE TABLE IF NOT EXISTS coro_test (id INTEGER PRIMARY KEY, "
            "name VARCHAR, value DOUBLE)");
        printTestResult("协程-创建表", true);

        // 2. 插入数据
        result = co_await client->execSqlCoro(
            "INSERT INTO coro_test VALUES (1, 'test1', 10.5), (2, 'test2', "
            "20.7)");
        printTestResult("协程-插入数据", result.affectedRows() == 2);

        // 3. 查询数据
        result =
            co_await client->execSqlCoro("SELECT * FROM coro_test WHERE id = ?",
                                         1);
        printTestResult("协程-参数化查询",
                        result.size() == 1 &&
                            result[0]["name"].as<std::string>() == "test1");

        // 4. JSON测试
        result = co_await client->execSqlCoro(
            "CREATE TABLE IF NOT EXISTS coro_json_test (id INTEGER, data "
            "JSON)");
        result = co_await client->execSqlCoro(
            "INSERT INTO coro_json_test VALUES (?, ?)",
            1,
            R"({"name": "Alice", "age": 30})");
        result = co_await client->execSqlCoro(
            "SELECT * FROM coro_json_test WHERE id = ?", 1);
        printTestResult("协程-JSON操作", result.size() == 1);

        // 5. 复杂查询
        result = co_await client->execSqlCoro(
            "SELECT COUNT(*) as count, AVG(value) as avg_value FROM coro_test");
        printTestResult("协程-聚合查询", result.size() == 1);

        // 6. 事务测试
        result = co_await client->execSqlCoro("BEGIN TRANSACTION");
        result = co_await client->execSqlCoro(
            "INSERT INTO coro_test VALUES (3, 'test3', 30.0)");
        result = co_await client->execSqlCoro("COMMIT");
        printTestResult("协程-事务操作", true);
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << "协程测试失败: " << e.base().what();
        printTestResult("协程测试", false);
    }
}
#endif

// 基本功能测试
void testBasicOperations(DbClientPtr client)
{
    LOG_INFO << "\n=== 基本功能测试 ===";

    // 1. 创建表
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS basic_test (id INTEGER PRIMARY KEY, name "
        "VARCHAR, age INTEGER, salary DOUBLE)",
        [](const Result &result) { printTestResult("创建表", true); },
        [](const std::exception_ptr &e) { printTestResult("创建表", false); });

    // 2. 插入数据
    client->execSqlAsync(
        "INSERT INTO basic_test VALUES (?, ?, ?, ?)",
        [](const Result &result) {
            printTestResult("插入数据", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) { printTestResult("插入数据", false); },
        1,
        "Alice",
        25,
        50000.0);

    // 3. 查询数据
    client->execSqlAsync(
        "SELECT * FROM basic_test WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1 &&
                           result[0]["name"].as<std::string>() == "Alice" &&
                           result[0]["age"].as<int>() == 25;
            printTestResult("查询数据", success);
        },
        [](const std::exception_ptr &e) { printTestResult("查询数据", false); },
        1);

    // 4. 更新数据
    client->execSqlAsync(
        "UPDATE basic_test SET salary = ? WHERE id = ?",
        [](const Result &result) {
            printTestResult("更新数据", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) { printTestResult("更新数据", false); },
        55000.0,
        1);

    // 5. 删除数据
    client->execSqlAsync(
        "DELETE FROM basic_test WHERE id = ?",
        [](const Result &result) {
            printTestResult("删除数据", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) { printTestResult("删除数据", false); },
        1);
}

// JSON功能测试
void testJsonOperations(DbClientPtr client)
{
    LOG_INFO << "\n=== JSON功能测试 ===";

    // 1. 创建JSON表
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS json_test (id INTEGER PRIMARY KEY, data "
        "JSON, metadata JSON)",
        [](const Result &result) { printTestResult("创建JSON表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建JSON表", false);
        });

    // 2. 插入JSON数据
    std::string jsonData =
        R"({"name": "John", "age": 30, "city": "New York", "hobbies": ["reading", "swimming"]})";
    std::string metadata = R"({"created": "2024-01-01", "version": 1})";

    client->execSqlAsync(
        "INSERT INTO json_test VALUES (?, ?, ?)",
        [](const Result &result) {
            printTestResult("插入JSON数据", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入JSON数据", false);
        },
        1,
        jsonData,
        metadata);

    // 3. 查询JSON数据
    client->execSqlAsync(
        "SELECT * FROM json_test WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1 && !result[0]["data"].isNull() &&
                           !result[0]["metadata"].isNull();
            printTestResult("查询JSON数据", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("查询JSON数据", false);
        },
        1);

    // 4. JSON路径查询
    client->execSqlAsync(
        "SELECT data->>'$.name' as name, data->>'$.age' as age FROM json_test "
        "WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1 &&
                           result[0]["name"].as<std::string>() == "John";
            printTestResult("JSON路径查询", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("JSON路径查询", false);
        },
        1);

    // 5. 使用SqlBinder插入JSON
    {
        auto binder = (*client) << "INSERT INTO json_test VALUES (?, ?, ?)";
        binder << 2 << R"({"name": "Jane", "age": 25, "city": "London"})"
               << R"({"created": "2024-01-02", "version": 1})";
        binder >> [](const Result &result) {
            printTestResult("SqlBinder插入JSON", result.affectedRows() == 1);
        };
        binder >> [](const std::exception_ptr &e) {
            printTestResult("SqlBinder插入JSON", false);
        };
        binder.exec();
    }
}

// 数据类型测试
void testDataTypes(DbClientPtr client)
{
    LOG_INFO << "\n=== 数据类型测试 ===";

    // 1. 创建包含各种数据类型的表
    client->execSqlAsync(R"(
        CREATE TABLE IF NOT EXISTS data_types_test (
            id INTEGER PRIMARY KEY,
            int_val INTEGER,
            bigint_val BIGINT,
            double_val DOUBLE,
            text_val VARCHAR,
            bool_val BOOLEAN,
            date_val DATE,
            time_val TIME,
            timestamp_val TIMESTAMP,
            blob_val BLOB
        )
    )",
                         [](const Result &result) {
                             printTestResult("创建数据类型表", true);
                         },
                         [](const std::exception_ptr &e) {
                             printTestResult("创建数据类型表", false);
                         });

    // 2. 插入各种类型的数据
    std::string blobData = "binary data test";
    client->execSqlAsync(
        "INSERT INTO data_types_test VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        [](const Result &result) {
            printTestResult("插入各种数据类型", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入各种数据类型", false);
        },
        1,                      // id
        42,                     // int_val
        123456789012345,        // bigint_val
        3.14159,                // double_val
        "Hello World",          // text_val
        true,                   // bool_val
        "2024-01-01",           // date_val
        "12:34:56",             // time_val
        "2024-01-01 12:34:56",  // timestamp_val
        blobData                // blob_val
    );

    // 3. 查询并验证数据类型
    client->execSqlAsync(
        "SELECT * FROM data_types_test WHERE id = ?",
        [](const Result &result) {
            if (result.size() == 1)
            {
                auto row = result[0];
                bool success =
                    row["int_val"].as<int>() == 42 &&
                    row["bigint_val"].as<int64_t>() == 123456789012345 &&
                    std::abs(row["double_val"].as<double>() - 3.14159) <
                        0.0001 &&
                    row["text_val"].as<std::string>() == "Hello World" &&
                    row["bool_val"].as<bool>() == true;
                printTestResult("验证数据类型", success);
            }
            else
            {
                printTestResult("验证数据类型", false);
            }
        },
        [](const std::exception_ptr &e) {
            printTestResult("验证数据类型", false);
        },
        1);
}

// 错误处理测试
void testErrorHandling(DbClientPtr client)
{
    LOG_INFO << "\n=== 错误处理测试 ===";

    // 1. 语法错误
    client->execSqlAsync(
        "SELECT * FROM non_existent_table",
        [](const Result &result) {
            printTestResult("语法错误处理", false);  // 不应该成功
        },
        [](const std::exception_ptr &e) {
            printTestResult("语法错误处理", true);  // 应该捕获异常
        });

    // 2. 约束违反
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS constraint_test (id INTEGER PRIMARY KEY, "
        "name VARCHAR UNIQUE)",
        [](const Result &result) { printTestResult("创建约束表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建约束表", false);
        });

    client->execSqlAsync(
        "INSERT INTO constraint_test VALUES (?, ?)",
        [](const Result &result) {
            printTestResult("插入第一条数据", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入第一条数据", false);
        },
        1,
        "test");

    // 尝试插入重复的主键
    client->execSqlAsync(
        "INSERT INTO constraint_test VALUES (?, ?)",
        [](const Result &result) { printTestResult("主键约束违反", false); },
        [](const std::exception_ptr &e) {
            printTestResult("主键约束违反", true);
        },
        1,
        "test2");

    // 尝试插入重复的唯一值
    client->execSqlAsync(
        "INSERT INTO constraint_test VALUES (?, ?)",
        [](const Result &result) { printTestResult("唯一约束违反", false); },
        [](const std::exception_ptr &e) {
            printTestResult("唯一约束违反", true);
        },
        2,
        "test");
}

// 并发测试
void testConcurrency(DbClientPtr client)
{
    LOG_INFO << "\n=== 并发测试 ===";

    // 创建测试表
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS concurrency_test (id INTEGER PRIMARY KEY, "
        "value INTEGER)",
        [](const Result &result) { printTestResult("创建并发测试表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建并发测试表", false);
        });

    // 并发插入测试
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> totalCount{0};

    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([client, i, &successCount, &totalCount]() {
            client->execSqlAsync(
                "INSERT INTO concurrency_test VALUES (?, ?)",
                [&successCount](const Result &result) { successCount++; },
                [](const std::exception_ptr &e) {
                    // 失败处理
                },
                i + 100,
                i * 10);
            totalCount++;
        });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    // 等待所有操作完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printTestResult("并发插入测试", successCount.load() == 10);

    // 并发查询测试
    successCount = 0;
    totalCount = 0;
    threads.clear();

    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([client, i, &successCount, &totalCount]() {
            client->execSqlAsync(
                "SELECT * FROM concurrency_test WHERE id = ?",
                [&successCount](const Result &result) {
                    if (result.size() == 1)
                    {
                        successCount++;
                    }
                },
                [](const std::exception_ptr &e) {
                    // 失败处理
                },
                i + 100);
            totalCount++;
        });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printTestResult("并发查询测试", successCount.load() == 10);
}

// 性能测试
void testPerformance(DbClientPtr client)
{
    LOG_INFO << "\n=== 性能测试 ===";

    // 创建性能测试表
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS performance_test (id INTEGER PRIMARY KEY, "
        "name VARCHAR, value DOUBLE)",
        [](const Result &result) { printTestResult("创建性能测试表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建性能测试表", false);
        });

    // 批量插入性能测试
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        client->execSqlAsync(
            "INSERT INTO performance_test VALUES (?, ?, ?)",
            [](const Result &result) {
                // 成功回调
            },
            [](const std::exception_ptr &e) {
                // 失败回调
            },
            i,
            "test" + std::to_string(i),
            i * 1.5);
    }

    // 等待所有插入完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    printTestResult("批量插入性能测试",
                    duration.count() < 1000);  // 应该在1秒内完成

    // 查询性能测试
    start = std::chrono::high_resolution_clock::now();

    client->execSqlAsync(
        "SELECT COUNT(*) as count, AVG(value) as avg_value FROM "
        "performance_test",
        [start](const Result &result) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);
            bool success = result.size() == 1 &&
                           result[0]["count"].as<int>() == 1000 &&
                           duration.count() < 100;  // 应该在100ms内完成
            printTestResult("查询性能测试", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("查询性能测试", false);
        });
}

// 高级功能测试
void testAdvancedFeatures(DbClientPtr client)
{
    LOG_INFO << "\n=== 高级功能测试 ===";

    // 1. 视图测试
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS view_source (id INTEGER, name VARCHAR, "
        "category VARCHAR)",
        [](const Result &result) { printTestResult("创建视图源表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建视图源表", false);
        });

    client->execSqlAsync(
        "INSERT INTO view_source VALUES (1, 'Item1', 'A'), (2, 'Item2', 'B'), "
        "(3, 'Item3', 'A')",
        [](const Result &result) {
            printTestResult("插入视图数据", result.affectedRows() == 3);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入视图数据", false);
        });

    client->execSqlAsync(
        "CREATE VIEW category_view AS SELECT category, COUNT(*) as count FROM "
        "view_source GROUP BY category",
        [](const Result &result) { printTestResult("创建视图", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建视图", false);
        });

    client->execSqlAsync(
        "SELECT * FROM category_view",
        [](const Result &result) {
            printTestResult("查询视图", result.size() == 2);
        },
        [](const std::exception_ptr &e) {
            printTestResult("查询视图", false);
        });

    // 2. 复杂JSON查询
    client->execSqlAsync(
        "CREATE TABLE IF NOT EXISTS complex_json (id INTEGER, data JSON)",
        [](const Result &result) { printTestResult("创建复杂JSON表", true); },
        [](const std::exception_ptr &e) {
            printTestResult("创建复杂JSON表", false);
        });

    std::string complexJson = R"({
        "user": {
            "name": "John Doe",
            "age": 30,
            "address": {
                "street": "123 Main St",
                "city": "New York",
                "zip": "10001"
            },
            "hobbies": ["reading", "swimming", "coding"]
        },
        "metadata": {
            "created": "2024-01-01",
            "version": 1,
            "tags": ["test", "example"]
        }
    })";

    client->execSqlAsync(
        "INSERT INTO complex_json VALUES (?, ?)",
        [](const Result &result) {
            printTestResult("插入复杂JSON", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入复杂JSON", false);
        },
        1,
        complexJson);

    client->execSqlAsync(
        "SELECT data->>'$.user.name' as name, data->>'$.user.age' as age FROM "
        "complex_json WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1 &&
                           result[0]["name"].as<std::string>() == "John Doe" &&
                           result[0]["age"].as<int>() == 30;
            printTestResult("复杂JSON查询", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("复杂JSON查询", false);
        },
        1);
}

int main(int argc, char *argv[])
{
    app().setLogLevel(trantor::Logger::kDebug);

    // 创建DuckDB客户端
    auto client =
        drogon::orm::DbClient::newDuckDbClient("filename=comprehensive_test.db",
                                               1);

#ifdef __cpp_impl_coroutine
    if (argc > 1 && std::string_view(argv[1]) == "--coro")
    {
        LOG_INFO << "运行协程模式测试";
        app().getLoop()->queueInLoop(
            [client]() { comprehensiveCoroTest(client); });
        app().run();
        printTestSummary();
        return 0;
    }
#endif

    LOG_INFO << "运行全面测试套件";

    // 运行各种测试
    testBasicOperations(client);
    testJsonOperations(client);
    testDataTypes(client);
    testErrorHandling(client);
    testConcurrency(client);
    testPerformance(client);
    testAdvancedFeatures(client);

    // 等待所有异步操作完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    printTestSummary();

    app().run();
    app().quit();
    return 0;
}
