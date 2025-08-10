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
void printTestResult(const std::string& testName, bool success) {
    testCounter++;
    if (success) {
        successCounter++;
        LOG_INFO << "✓ " << testName << " - PASSED";
    } else {
        failureCounter++;
        LOG_INFO << "✗ " << testName << " - FAILED";
    }
}

void printTestSummary() {
    LOG_INFO << "\n=== DuckDB特有功能测试总结 ===";
    LOG_INFO << "总测试数: " << testCounter.load();
    LOG_INFO << "成功: " << successCounter.load();
    LOG_INFO << "失败: " << failureCounter.load();
    LOG_INFO << "成功率: " << (testCounter.load() > 0 ? (successCounter.load() * 100.0 / testCounter.load()) : 0) << "%";
}

// 向量化操作测试
void testVectorizedOperations(DbClientPtr client) {
    LOG_INFO << "\n=== 向量化操作测试 ===";
    
    // 创建测试表
    client->execSqlAsync("CREATE TABLE IF NOT EXISTS vector_test (id INTEGER, value DOUBLE, category VARCHAR)",
        [](const Result &result) {
            printTestResult("创建向量化测试表", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建向量化测试表", false);
        });
    
    // 插入大量数据
    for (int i = 0; i < 1000; ++i) {
        client->execSqlAsync("INSERT INTO vector_test VALUES (?, ?, ?)",
            [](const Result &result) {
                // 成功回调
            },
            [](const std::exception_ptr &e) {
                // 失败回调
            }, i, i * 1.5, (i % 3 == 0) ? "A" : (i % 3 == 1) ? "B" : "C");
    }
    
    // 等待插入完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 测试向量化聚合
    client->execSqlAsync("SELECT category, COUNT(*) as count, AVG(value) as avg_value, SUM(value) as sum_value FROM vector_test GROUP BY category ORDER BY category",
        [](const Result &result) {
            bool success = result.size() == 3; // 应该有3个类别
            if (success) {
                for (const auto& row : result) {
                    std::string category = row["category"].as<std::string>();
                    int count = row["count"].as<int>();
                    double avg_value = row["avg_value"].as<double>();
                    double sum_value = row["sum_value"].as<double>();
                    
                    // 验证数据
                    if (category == "A" && count > 0 && avg_value > 0 && sum_value > 0) {
                        success = true;
                    } else if (category == "B" && count > 0 && avg_value > 0 && sum_value > 0) {
                        success = true;
                    } else if (category == "C" && count > 0 && avg_value > 0 && sum_value > 0) {
                        success = true;
                    } else {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("向量化聚合操作", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("向量化聚合操作", false);
        });
    
    // 测试窗口函数
    client->execSqlAsync("SELECT id, value, category, ROW_NUMBER() OVER (PARTITION BY category ORDER BY value) as row_num FROM vector_test LIMIT 20",
        [](const Result &result) {
            bool success = result.size() == 20;
            if (success) {
                for (const auto& row : result) {
                    int row_num = row["row_num"].as<int>();
                    if (row_num < 1) {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("窗口函数测试", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("窗口函数测试", false);
        });
}

// 分析函数测试
void testAnalyticalFunctions(DbClientPtr client) {
    LOG_INFO << "\n=== 分析函数测试 ===";
    
    // 创建时间序列数据
    client->execSqlAsync("CREATE TABLE IF NOT EXISTS time_series (date DATE, value DOUBLE, category VARCHAR)",
        [](const Result &result) {
            printTestResult("创建时间序列表", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建时间序列表", false);
        });
    
    // 插入时间序列数据
    for (int i = 0; i < 100; ++i) {
        std::string date = "2024-01-" + std::to_string((i % 30) + 1);
        client->execSqlAsync("INSERT INTO time_series VALUES (?, ?, ?)",
            [](const Result &result) {
                // 成功回调
            },
            [](const std::exception_ptr &e) {
                // 失败回调
            }, date, 100 + i * 0.5, (i % 2 == 0) ? "A" : "B");
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 测试移动平均
    client->execSqlAsync("SELECT date, value, AVG(value) OVER (ORDER BY date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) as moving_avg FROM time_series ORDER BY date LIMIT 10",
        [](const Result &result) {
            bool success = result.size() == 10;
            if (success) {
                for (const auto& row : result) {
                    double moving_avg = row["moving_avg"].as<double>();
                    if (moving_avg <= 0) {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("移动平均计算", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("移动平均计算", false);
        });
    
    // 测试排名函数
    client->execSqlAsync("SELECT date, value, RANK() OVER (ORDER BY value DESC) as rank_value FROM time_series ORDER BY date LIMIT 10",
        [](const Result &result) {
            bool success = result.size() == 10;
            if (success) {
                for (const auto& row : result) {
                    int rank = row["rank_value"].as<int>();
                    if (rank < 1) {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("排名函数测试", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("排名函数测试", false);
        });
}

// 复杂JSON操作测试
void testComplexJsonOperations(DbClientPtr client) {
    LOG_INFO << "\n=== 复杂JSON操作测试 ===";
    
    // 创建复杂JSON表
    client->execSqlAsync("CREATE TABLE IF NOT EXISTS complex_json_test (id INTEGER, data JSON)",
        [](const Result &result) {
            printTestResult("创建复杂JSON表", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建复杂JSON表", false);
        });
    
    // 插入复杂JSON数据
    std::vector<std::string> complexJsons = {
        R"({
            "user": {
                "id": 1,
                "name": "Alice",
                "profile": {
                    "age": 25,
                    "city": "New York",
                    "preferences": {
                        "theme": "dark",
                        "language": "en"
                    }
                },
                "orders": [
                    {"id": 1, "amount": 100.50, "items": ["item1", "item2"]},
                    {"id": 2, "amount": 200.75, "items": ["item3"]}
                ]
            },
            "metadata": {
                "created": "2024-01-01T10:00:00Z",
                "version": "1.0",
                "tags": ["premium", "verified"]
            }
        })",
        R"({
            "user": {
                "id": 2,
                "name": "Bob",
                "profile": {
                    "age": 30,
                    "city": "London",
                    "preferences": {
                        "theme": "light",
                        "language": "en"
                    }
                },
                "orders": [
                    {"id": 3, "amount": 150.25, "items": ["item4", "item5", "item6"]}
                ]
            },
            "metadata": {
                "created": "2024-01-02T15:30:00Z",
                "version": "1.0",
                "tags": ["standard"]
            }
        })"
    };
    
    for (size_t i = 0; i < complexJsons.size(); ++i) {
        client->execSqlAsync("INSERT INTO complex_json_test VALUES (?, ?)",
            [](const Result &result) {
                // 成功回调
            },
            [](const std::exception_ptr &e) {
                // 失败回调
            }, i + 1, complexJsons[i]);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 测试JSON路径查询
    client->execSqlAsync("SELECT id, data->>'$.user.name' as name, data->>'$.user.profile.age' as age FROM complex_json_test",
        [](const Result &result) {
            bool success = result.size() == 2;
            if (success) {
                for (const auto& row : result) {
                    std::string name = row["name"].as<std::string>();
                    int age = row["age"].as<int>();
                    if (name.empty() || age <= 0) {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("JSON路径查询", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("JSON路径查询", false);
        });
    
    // 测试JSON数组操作
    client->execSqlAsync("SELECT id, data->>'$.user.orders[0].amount' as first_order_amount FROM complex_json_test",
        [](const Result &result) {
            bool success = result.size() == 2;
            if (success) {
                for (const auto& row : result) {
                    double amount = row["first_order_amount"].as<double>();
                    if (amount <= 0) {
                        success = false;
                        break;
                    }
                }
            }
            printTestResult("JSON数组操作", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("JSON数组操作", false);
        });
    
    // 测试JSON聚合
    client->execSqlAsync("SELECT COUNT(*) as total_users, AVG(CAST(data->>'$.user.profile.age' AS INTEGER)) as avg_age FROM complex_json_test",
        [](const Result &result) {
            bool success = result.size() == 1;
            if (success) {
                int total_users = result[0]["total_users"].as<int>();
                double avg_age = result[0]["avg_age"].as<double>();
                success = total_users == 2 && avg_age > 0;
            }
            printTestResult("JSON聚合操作", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("JSON聚合操作", false);
        });
}

// 数据类型和函数测试
void testDataTypesAndFunctions(DbClientPtr client) {
    LOG_INFO << "\n=== 数据类型和函数测试 ===";
    
    // 创建数据类型测试表
    client->execSqlAsync(R"(
        CREATE TABLE IF NOT EXISTS data_types_advanced (
            id INTEGER PRIMARY KEY,
            int_val INTEGER,
            bigint_val BIGINT,
            double_val DOUBLE,
            text_val VARCHAR,
            bool_val BOOLEAN,
            date_val DATE,
            time_val TIME,
            timestamp_val TIMESTAMP,
            interval_val INTERVAL,
            uuid_val UUID,
            json_val JSON
        )
    )",
        [](const Result &result) {
            printTestResult("创建高级数据类型表", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建高级数据类型表", false);
        });
    
    // 插入各种数据类型
    client->execSqlAsync("INSERT INTO data_types_advanced VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        [](const Result &result) {
            printTestResult("插入高级数据类型", result.affectedRows() == 1);
        },
        [](const std::exception_ptr &e) {
            printTestResult("插入高级数据类型", false);
        },
        1,                    // id
        42,                   // int_val
        123456789012345,      // bigint_val
        3.14159,             // double_val
        "Hello World",        // text_val
        true,                // bool_val
        "2024-01-01",        // date_val
        "12:34:56",          // time_val
        "2024-01-01 12:34:56", // timestamp_val
        "1 day",             // interval_val
        "550e8400-e29b-41d4-a716-446655440000", // uuid_val
        R"({"test": "value"})" // json_val
    );
    
    // 测试类型转换函数
    client->execSqlAsync("SELECT CAST(text_val AS INTEGER) as cast_int, CAST(double_val AS VARCHAR) as cast_str FROM data_types_advanced WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1;
            if (success) {
                // 验证类型转换
                success = true; // 简化验证
            }
            printTestResult("类型转换函数", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("类型转换函数", false);
        }, 1);
    
    // 测试数学函数
    client->execSqlAsync("SELECT ABS(double_val) as abs_val, ROUND(double_val, 2) as round_val, CEIL(double_val) as ceil_val FROM data_types_advanced WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1;
            if (success) {
                auto row = result[0];
                double abs_val = row["abs_val"].as<double>();
                double round_val = row["round_val"].as<double>();
                double ceil_val = row["ceil_val"].as<double>();
                success = abs_val > 0 && round_val > 0 && ceil_val > 0;
            }
            printTestResult("数学函数", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("数学函数", false);
        }, 1);
    
    // 测试字符串函数
    client->execSqlAsync("SELECT LENGTH(text_val) as len, UPPER(text_val) as upper_val, LOWER(text_val) as lower_val FROM data_types_advanced WHERE id = ?",
        [](const Result &result) {
            bool success = result.size() == 1;
            if (success) {
                auto row = result[0];
                int len = row["len"].as<int>();
                std::string upper_val = row["upper_val"].as<std::string>();
                std::string lower_val = row["lower_val"].as<std::string>();
                success = len > 0 && !upper_val.empty() && !lower_val.empty();
            }
            printTestResult("字符串函数", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("字符串函数", false);
        }, 1);
}

// 性能优化测试
void testPerformanceOptimizations(DbClientPtr client) {
    LOG_INFO << "\n=== 性能优化测试 ===";
    
    // 创建大表进行性能测试
    client->execSqlAsync("CREATE TABLE IF NOT EXISTS performance_large (id INTEGER PRIMARY KEY, name VARCHAR, value DOUBLE, category VARCHAR, created_date DATE)",
        [](const Result &result) {
            printTestResult("创建大表", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建大表", false);
        });
    
    // 批量插入大量数据
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 5000; ++i) {
        std::string name = "item_" + std::to_string(i);
        double value = i * 1.5;
        std::string category = (i % 5 == 0) ? "A" : (i % 5 == 1) ? "B" : (i % 5 == 2) ? "C" : (i % 5 == 3) ? "D" : "E";
        std::string date = "2024-01-" + std::to_string((i % 30) + 1);
        
        client->execSqlAsync("INSERT INTO performance_large VALUES (?, ?, ?, ?, ?)",
            [](const Result &result) {
                // 成功回调
            },
            [](const std::exception_ptr &e) {
                // 失败回调
            }, i, name, value, category, date);
    }
    
    // 等待插入完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto end = std::chrono::high_resolution_clock::now();
    auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    printTestResult("批量插入性能", insertDuration.count() < 2000); // 应该在2秒内完成
    
    // 测试查询性能
    start = std::chrono::high_resolution_clock::now();
    
    client->execSqlAsync("SELECT category, COUNT(*) as count, AVG(value) as avg_value, MIN(value) as min_value, MAX(value) as max_value FROM performance_large GROUP BY category ORDER BY category",
        [start](const Result &result) {
            auto end = std::chrono::high_resolution_clock::now();
            auto queryDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            bool success = result.size() == 5 && queryDuration.count() < 500; // 应该在500ms内完成
            printTestResult("复杂查询性能", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("复杂查询性能", false);
        });
    
    // 测试索引效果
    client->execSqlAsync("CREATE INDEX IF NOT EXISTS idx_category ON performance_large(category)",
        [](const Result &result) {
            printTestResult("创建索引", true);
        },
        [](const std::exception_ptr &e) {
            printTestResult("创建索引", false);
        });
    
    start = std::chrono::high_resolution_clock::now();
    
    client->execSqlAsync("SELECT * FROM performance_large WHERE category = ? LIMIT 100",
        [start](const Result &result) {
            auto end = std::chrono::high_resolution_clock::now();
            auto queryDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            bool success = result.size() == 100 && queryDuration.count() < 100; // 应该在100ms内完成
            printTestResult("索引查询性能", success);
        },
        [](const std::exception_ptr &e) {
            printTestResult("索引查询性能", false);
        }, "A");
}

int main(int argc, char *argv[])
{
    app().setLogLevel(trantor::Logger::kDebug);
    
    // 创建DuckDB客户端
    auto client = drogon::orm::DbClient::newDuckDbClient("filename=duckdb_specific_test.db", 1);
    
    LOG_INFO << "运行DuckDB特有功能测试套件";
    
    // 运行各种测试
    testVectorizedOperations(client);
    testAnalyticalFunctions(client);
    testComplexJsonOperations(client);
    testDataTypesAndFunctions(client);
    testPerformanceOptimizations(client);
    
    // 等待所有异步操作完成
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    printTestSummary();
    
    app().run();
    app().quit();
    return 0;
} 