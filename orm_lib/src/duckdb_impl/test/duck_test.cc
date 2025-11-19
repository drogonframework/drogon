#include "drogon/drogon.h"
#include "drogon/orm/DbClient.h"
#include <iostream>
#include <trantor/utils/Logger.h>
#include <cassert>
#include <vector>
#include <map>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <thread>
#include <chrono>
#include <future>

using namespace std::chrono_literals;
using namespace drogon::orm;

// Test result tracking
struct TestResults {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
    
    void recordPass(const std::string& testName) {
        passed++;
        LOG_DEBUG << "✓ PASS: " << testName;
    }
    
    void recordFail(const std::string& testName, const std::string& reason = "") {
        failed++;
        std::string msg = "✗ FAIL: " + testName;
        if (!reason.empty()) {
            msg += " - " + reason;
        }
        failures.push_back(msg);
        LOG_ERROR << msg;
    }
    
    void printSummary() {
        LOG_INFO << "\n=== Test Summary ===";
        LOG_INFO << "Passed: " << passed;
        LOG_INFO << "Failed: " << failed;
        if (!failures.empty()) {
            LOG_ERROR << "\nFailures:";
            for (const auto& f : failures) {
                LOG_ERROR << "  " << f;
            }
        }
    }
};

TestResults testResults;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);

    // Create DuckDB client
    auto clientPtr = DbClient::newDuckDbClient("filename=test_duckdb_comprehensive.db", 1);
    std::this_thread::sleep_for(1s);

    LOG_INFO << "\n==========================================";
    LOG_INFO << "DuckDB Comprehensive Test Suite";
    LOG_INFO << "==========================================\n";

    // ============================================
    // Test 1: Basic Data Types
    // ============================================
    LOG_INFO << "\n[Test 1] Testing Basic Data Types...";
    *clientPtr << "DROP TABLE IF EXISTS test_data_types" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_data_types ("
                  "id INTEGER PRIMARY KEY,"
                  "name VARCHAR(100),"
                  "age INTEGER,"
                  "score DOUBLE,"
                  "is_active BOOLEAN,"
                  "salary DECIMAL(10,2),"
                  "birth_date DATE,"
                  "created_at TIMESTAMP,"
                  "data BLOB,"
                  "notes TEXT)"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Create table with all data types");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Create table with all data types", e.base().what());
        };

    // Insert with all data types
    std::string blobData = "binary data test";
    *clientPtr << "INSERT INTO test_data_types (id, name, age, score, is_active, salary, birth_date, created_at, data, notes) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
               << 1 << "Alice" << 25 << 95.5 << true << 50000.50 
               << "1998-01-15" << "2024-01-01 10:30:00" << blobData << "Test notes"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Insert with all data types");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Insert with all data types", e.base().what());
        };

    // Query and verify data types
    *clientPtr << "SELECT * FROM test_data_types WHERE id = 1" >>
        [](const Result &r) {
            if (r.size() == 1) {
                auto row = r[0];
                assert(row["id"].as<int>() == 1);
                assert(row["name"].as<std::string>() == "Alice");
                assert(row["age"].as<int>() == 25);
                assert(std::abs(row["score"].as<double>() - 95.5) < 0.01);
                assert(row["is_active"].as<bool>() == true);
                testResults.recordPass("Query and verify all data types");
            } else {
                testResults.recordFail("Query and verify all data types", "No rows returned");
            }
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Query and verify all data types", e.base().what());
        };

    // ============================================
    // Test 2: NULL Handling
    // ============================================
    LOG_INFO << "\n[Test 2] Testing NULL Handling...";
    *clientPtr << "INSERT INTO test_data_types (id, name, age, score, is_active) VALUES (?, ?, ?, ?, ?)"
               << 2 << "Bob" << nullptr << nullptr << nullptr
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Insert with NULL values");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Insert with NULL values", e.base().what());
        };

    *clientPtr << "SELECT * FROM test_data_types WHERE id = 2" >>
        [](const Result &r) {
            if (r.size() == 1) {
                auto row = r[0];
                assert(!row["id"].isNull());
                assert(!row["name"].isNull());
                assert(row["age"].isNull());
                assert(row["score"].isNull());
                assert(row["is_active"].isNull());
                testResults.recordPass("Query and verify NULL values");
            } else {
                testResults.recordFail("Query and verify NULL values", "No rows returned");
            }
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Query and verify NULL values", e.base().what());
        };

    // ============================================
    // Test 3: CRUD Operations
    // ============================================
    LOG_INFO << "\n[Test 3] Testing CRUD Operations...";
    *clientPtr << "DROP TABLE IF EXISTS test_crud" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_crud (id INTEGER PRIMARY KEY, name VARCHAR(50), value INTEGER)"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Create table for CRUD test");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Create table for CRUD test", e.base().what());
        };

    // Create (Insert)
    *clientPtr << "INSERT INTO test_crud (id, name, value) VALUES (?, ?, ?)"
               << 1 << "Item1" << 100
               << Mode::Blocking >>
        [](const Result &r) {
            assert(r.affectedRows() == 1);
            testResults.recordPass("INSERT operation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("INSERT operation", e.base().what());
        };

    // Read (Select)
    *clientPtr << "SELECT * FROM test_crud WHERE id = ?" << 1 >>
        [](const Result &r) {
            assert(r.size() == 1);
            assert(r[0]["name"].as<std::string>() == "Item1");
            testResults.recordPass("SELECT operation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("SELECT operation", e.base().what());
        };

    // Update
    *clientPtr << "UPDATE test_crud SET value = ? WHERE id = ?"
               << 200 << 1
               << Mode::Blocking >>
        [](const Result &r) {
            assert(r.affectedRows() == 1);
            testResults.recordPass("UPDATE operation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("UPDATE operation", e.base().what());
        };

    // Verify update
    *clientPtr << "SELECT value FROM test_crud WHERE id = ?" << 1 >>
        [](const Result &r) {
            assert(r.size() == 1);
            assert(r[0]["value"].as<int>() == 200);
            testResults.recordPass("Verify UPDATE");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Verify UPDATE", e.base().what());
        };

    // Delete
    *clientPtr << "DELETE FROM test_crud WHERE id = ?"
               << 1
               << Mode::Blocking >>
        [](const Result &r) {
            assert(r.affectedRows() == 1);
            testResults.recordPass("DELETE operation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("DELETE operation", e.base().what());
        };

    // Verify delete
    *clientPtr << "SELECT * FROM test_crud WHERE id = ?" << 1 >>
        [](const Result &r) {
            assert(r.size() == 0);
            testResults.recordPass("Verify DELETE");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Verify DELETE", e.base().what());
        };

    // ============================================
    // Test 4: Transactions
    // ============================================
    LOG_INFO << "\n[Test 4] Testing Transactions...";
    *clientPtr << "DROP TABLE IF EXISTS test_transactions" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_transactions (id INTEGER PRIMARY KEY, amount INTEGER)"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    // Test successful transaction
    {
        auto trans = clientPtr->newTransaction([](bool success) {
            if (success) {
                testResults.recordPass("Transaction commit");
            } else {
                testResults.recordFail("Transaction commit");
            }
        });

        *trans << "INSERT INTO test_transactions (id, amount) VALUES (?, ?)"
               << 1 << 100
               << Mode::Blocking >>
            [](const Result &r) {} >>
            [](const DrogonDbException &e) {};

        *trans << "INSERT INTO test_transactions (id, amount) VALUES (?, ?)"
               << 2 << 200
               << Mode::Blocking >>
            [](const Result &r) {} >>
            [](const DrogonDbException &e) {};

        trans->execSqlAsync(
            "SELECT COUNT(*) as cnt FROM test_transactions",
            [trans](const Result &r) {
                assert(r[0]["cnt"].as<int>() == 2);
                testResults.recordPass("Transaction - multiple inserts");
            },
            [](const DrogonDbException &e) {
                testResults.recordFail("Transaction - multiple inserts", e.base().what());
            });
    }

    std::this_thread::sleep_for(500ms);

    // Verify transaction committed
    *clientPtr << "SELECT COUNT(*) as cnt FROM test_transactions" >>
        [](const Result &r) {
            assert(r[0]["cnt"].as<int>() == 2);
            testResults.recordPass("Verify transaction commit");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Verify transaction commit", e.base().what());
        };

    // ============================================
    // Test 5: Joins
    // ============================================
    LOG_INFO << "\n[Test 5] Testing Joins...";
    *clientPtr << "DROP TABLE IF EXISTS test_orders" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "DROP TABLE IF EXISTS test_customers" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_customers (id INTEGER PRIMARY KEY, name VARCHAR(50))"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_orders (id INTEGER PRIMARY KEY, customer_id INTEGER, amount INTEGER)"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "INSERT INTO test_customers (id, name) VALUES (1, 'Alice'), (2, 'Bob')"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "INSERT INTO test_orders (id, customer_id, amount) VALUES (1, 1, 100), (2, 1, 200), (3, 2, 150)"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    // INNER JOIN
    *clientPtr << "SELECT c.name, o.amount FROM test_customers c "
                  "INNER JOIN test_orders o ON c.id = o.customer_id" >>
        [](const Result &r) {
            assert(r.size() == 3);
            testResults.recordPass("INNER JOIN");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("INNER JOIN", e.base().what());
        };

    // LEFT JOIN
    *clientPtr << "SELECT c.name, o.amount FROM test_customers c "
                  "LEFT JOIN test_orders o ON c.id = o.customer_id" >>
        [](const Result &r) {
            assert(r.size() >= 2);
            testResults.recordPass("LEFT JOIN");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("LEFT JOIN", e.base().what());
        };

    // ============================================
    // Test 6: Aggregations
    // ============================================
    LOG_INFO << "\n[Test 6] Testing Aggregations...";
    *clientPtr << "DROP TABLE IF EXISTS test_sales" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_sales (id INTEGER PRIMARY KEY, product VARCHAR(50), amount INTEGER, category VARCHAR(50))"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "INSERT INTO test_sales (id, product, amount, category) VALUES "
                  "(1, 'Product1', 100, 'A'), "
                  "(2, 'Product2', 200, 'A'), "
                  "(3, 'Product3', 150, 'B'), "
                  "(4, 'Product4', 300, 'B')"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    // COUNT
    *clientPtr << "SELECT COUNT(*) as total FROM test_sales" >>
        [](const Result &r) {
            assert(r[0]["total"].as<int>() == 4);
            testResults.recordPass("COUNT aggregation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("COUNT aggregation", e.base().what());
        };

    // SUM
    *clientPtr << "SELECT SUM(amount) as total FROM test_sales" >>
        [](const Result &r) {
            assert(r[0]["total"].as<int>() == 750);
            testResults.recordPass("SUM aggregation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("SUM aggregation", e.base().what());
        };

    // AVG
    *clientPtr << "SELECT AVG(amount) as avg_amount FROM test_sales" >>
        [](const Result &r) {
            double avg = r[0]["avg_amount"].as<double>();
            assert(std::abs(avg - 187.5) < 0.1);
            testResults.recordPass("AVG aggregation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("AVG aggregation", e.base().what());
        };

    // MAX
    *clientPtr << "SELECT MAX(amount) as max_amount FROM test_sales" >>
        [](const Result &r) {
            assert(r[0]["max_amount"].as<int>() == 300);
            testResults.recordPass("MAX aggregation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("MAX aggregation", e.base().what());
        };

    // MIN
    *clientPtr << "SELECT MIN(amount) as min_amount FROM test_sales" >>
        [](const Result &r) {
            assert(r[0]["min_amount"].as<int>() == 100);
            testResults.recordPass("MIN aggregation");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("MIN aggregation", e.base().what());
        };

    // GROUP BY
    *clientPtr << "SELECT category, SUM(amount) as total FROM test_sales GROUP BY category" >>
        [](const Result &r) {
            assert(r.size() == 2);
            testResults.recordPass("GROUP BY");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("GROUP BY", e.base().what());
        };

    // HAVING
    *clientPtr << "SELECT category, SUM(amount) as total FROM test_sales "
                  "GROUP BY category HAVING SUM(amount) > 400" >>
        [](const Result &r) {
            assert(r.size() == 1);
            testResults.recordPass("HAVING clause");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("HAVING clause", e.base().what());
        };

    // ============================================
    // Test 7: Subqueries
    // ============================================
    LOG_INFO << "\n[Test 7] Testing Subqueries...";
    *clientPtr << "SELECT * FROM test_sales WHERE amount > (SELECT AVG(amount) FROM test_sales)" >>
        [](const Result &r) {
            assert(r.size() >= 1);
            testResults.recordPass("Subquery in WHERE clause");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Subquery in WHERE clause", e.base().what());
        };

    *clientPtr << "SELECT product, amount, (SELECT AVG(amount) FROM test_sales) as avg_amount FROM test_sales" >>
        [](const Result &r) {
            assert(r.size() == 4);
            testResults.recordPass("Subquery in SELECT clause");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Subquery in SELECT clause", e.base().what());
        };

    // ============================================
    // Test 8: Views
    // ============================================
    LOG_INFO << "\n[Test 8] Testing Views...";
    *clientPtr << "DROP VIEW IF EXISTS test_sales_view" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE VIEW test_sales_view AS SELECT product, amount FROM test_sales WHERE amount > 150"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Create VIEW");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Create VIEW", e.base().what());
        };

    *clientPtr << "SELECT * FROM test_sales_view" >>
        [](const Result &r) {
            assert(r.size() >= 1);
            testResults.recordPass("Query VIEW");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Query VIEW", e.base().what());
        };

    // ============================================
    // Test 9: Indexes
    // ============================================
    LOG_INFO << "\n[Test 9] Testing Indexes...";
    *clientPtr << "DROP INDEX IF EXISTS idx_test_sales_product" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE INDEX idx_test_sales_product ON test_sales(product)"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Create INDEX");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Create INDEX", e.base().what());
        };

    // ============================================
    // Test 10: Constraints
    // ============================================
    LOG_INFO << "\n[Test 10] Testing Constraints...";
    *clientPtr << "DROP TABLE IF EXISTS test_constraints" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_constraints ("
                  "id INTEGER PRIMARY KEY,"
                  "email VARCHAR(100) UNIQUE,"
                  "age INTEGER CHECK(age >= 0 AND age <= 150),"
                  "status VARCHAR(20) DEFAULT 'active')"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Create table with constraints");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Create table with constraints", e.base().what());
        };

    *clientPtr << "INSERT INTO test_constraints (id, email, age) VALUES (?, ?, ?)"
               << 1 << "test@example.com" << 25
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordPass("Insert with constraints");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Insert with constraints", e.base().what());
        };

    // Test UNIQUE constraint
    *clientPtr << "INSERT INTO test_constraints (id, email, age) VALUES (?, ?, ?)"
               << 2 << "test@example.com" << 30
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordFail("UNIQUE constraint", "Should have failed");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordPass("UNIQUE constraint violation caught");
        };

    // ============================================
    // Test 11: Prepared Statements
    // ============================================
    LOG_INFO << "\n[Test 11] Testing Prepared Statements...";
    *clientPtr << "DROP TABLE IF EXISTS test_prepared" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_prepared (id INTEGER PRIMARY KEY, name VARCHAR(50))"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    // Multiple inserts using same prepared statement pattern
    for (int i = 1; i <= 5; i++) {
        *clientPtr << "INSERT INTO test_prepared (id, name) VALUES (?, ?)"
                   << i << ("Item" + std::to_string(i))
                   << Mode::Blocking >>
            [](const Result &r) {} >>
            [](const DrogonDbException &e) {};
    }

    *clientPtr << "SELECT COUNT(*) as cnt FROM test_prepared" >>
        [](const Result &r) {
            assert(r[0]["cnt"].as<int>() == 5);
            testResults.recordPass("Prepared statements (multiple inserts)");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Prepared statements", e.base().what());
        };

    // ============================================
    // Test 12: Async Operations
    // ============================================
    LOG_INFO << "\n[Test 12] Testing Async Operations...";
    std::promise<bool> asyncPromise;
    auto asyncFuture = asyncPromise.get_future();

    clientPtr->execSqlAsync(
        "SELECT COUNT(*) as cnt FROM test_prepared",
        [&asyncPromise](const Result &r) {
            assert(r[0]["cnt"].as<int>() == 5);
            asyncPromise.set_value(true);
        },
        [&asyncPromise](const DrogonDbException &e) {
            asyncPromise.set_value(false);
        });

    if (asyncFuture.wait_for(2s) == std::future_status::ready) {
        if (asyncFuture.get()) {
            testResults.recordPass("Async SQL execution");
        } else {
            testResults.recordFail("Async SQL execution");
        }
    } else {
        testResults.recordFail("Async SQL execution", "Timeout");
    }

    // ============================================
    // Test 13: Error Handling
    // ============================================
    LOG_INFO << "\n[Test 13] Testing Error Handling...";
    
    // Invalid SQL
    *clientPtr << "SELECT * FROM non_existent_table" >>
        [](const Result &r) {
            testResults.recordFail("Error handling", "Should have failed");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordPass("Error handling - invalid table");
        };

    // Invalid data type
    *clientPtr << "INSERT INTO test_constraints (id, email, age) VALUES (?, ?, ?)"
               << 3 << "valid@example.com" << "invalid_age"
               << Mode::Blocking >>
        [](const Result &r) {
            testResults.recordFail("Error handling", "Should have failed");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordPass("Error handling - invalid data type");
        };

    // ============================================
    // Test 14: Complex Queries
    // ============================================
    LOG_INFO << "\n[Test 14] Testing Complex Queries...";
    
    // ORDER BY
    *clientPtr << "SELECT * FROM test_sales ORDER BY amount DESC" >>
        [](const Result &r) {
            assert(r.size() == 4);
            assert(r[0]["amount"].as<int>() == 300);
            testResults.recordPass("ORDER BY");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("ORDER BY", e.base().what());
        };

    // LIMIT and OFFSET
    *clientPtr << "SELECT * FROM test_sales ORDER BY amount LIMIT 2 OFFSET 1" >>
        [](const Result &r) {
            assert(r.size() == 2);
            testResults.recordPass("LIMIT and OFFSET");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("LIMIT and OFFSET", e.base().what());
        };

    // DISTINCT
    *clientPtr << "SELECT DISTINCT category FROM test_sales" >>
        [](const Result &r) {
            assert(r.size() == 2);
            testResults.recordPass("DISTINCT");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("DISTINCT", e.base().what());
        };

    // LIKE
    *clientPtr << "SELECT * FROM test_sales WHERE product LIKE 'Product%'" >>
        [](const Result &r) {
            assert(r.size() == 4);
            testResults.recordPass("LIKE operator");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("LIKE operator", e.base().what());
        };

    // IN clause
    *clientPtr << "SELECT * FROM test_sales WHERE amount IN (100, 200, 300)" >>
        [](const Result &r) {
            assert(r.size() == 3);
            testResults.recordPass("IN clause");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("IN clause", e.base().what());
        };

    // BETWEEN
    *clientPtr << "SELECT * FROM test_sales WHERE amount BETWEEN 150 AND 250" >>
        [](const Result &r) {
            assert(r.size() == 2);
            testResults.recordPass("BETWEEN operator");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("BETWEEN operator", e.base().what());
        };

    // ============================================
    // Test 15: Date and Time Functions
    // ============================================
    LOG_INFO << "\n[Test 15] Testing Date and Time Functions...";
    *clientPtr << "DROP TABLE IF EXISTS test_dates" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_dates (id INTEGER PRIMARY KEY, event_date DATE, event_time TIMESTAMP)"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "INSERT INTO test_dates (id, event_date, event_time) VALUES "
                  "(1, '2024-01-15', '2024-01-15 10:30:00'), "
                  "(2, '2024-02-20', '2024-02-20 14:45:00')"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "SELECT * FROM test_dates WHERE event_date >= '2024-02-01'" >>
        [](const Result &r) {
            assert(r.size() == 1);
            testResults.recordPass("Date comparison");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Date comparison", e.base().what());
        };

    // ============================================
    // Test 16: String Functions
    // ============================================
    LOG_INFO << "\n[Test 16] Testing String Functions...";
    *clientPtr << "SELECT UPPER(name) as upper_name, LOWER(name) as lower_name, "
                  "LENGTH(name) as name_length FROM test_customers" >>
        [](const Result &r) {
            assert(r.size() == 2);
            testResults.recordPass("String functions (UPPER, LOWER, LENGTH)");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("String functions", e.base().what());
        };

    // ============================================
    // Test 17: Multiple Connections
    // ============================================
    LOG_INFO << "\n[Test 17] Testing Multiple Connections...";
    auto clientPtr2 = DbClient::newDuckDbClient("filename=test_duckdb_comprehensive.db", 1);
    std::this_thread::sleep_for(500ms);

    *clientPtr2 << "SELECT COUNT(*) as cnt FROM test_sales" >>
        [](const Result &r) {
            assert(r[0]["cnt"].as<int>() == 4);
            testResults.recordPass("Multiple connections");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Multiple connections", e.base().what());
        };

    // ============================================
    // Test 18: execSqlSync
    // ============================================
    LOG_INFO << "\n[Test 18] Testing execSqlSync...";
    try {
        auto result = clientPtr->execSqlSync("SELECT COUNT(*) as cnt FROM test_sales");
        assert(result[0]["cnt"].as<int>() == 4);
        testResults.recordPass("execSqlSync");
    } catch (const DrogonDbException &e) {
        testResults.recordFail("execSqlSync", e.base().what());
    }

    // ============================================
    // Test 19: execSqlAsyncFuture
    // ============================================
    LOG_INFO << "\n[Test 19] Testing execSqlAsyncFuture...";
    try {
        auto future = clientPtr->execSqlAsyncFuture("SELECT COUNT(*) as cnt FROM test_sales");
        auto result = future.get();
        assert(result[0]["cnt"].as<int>() == 4);
        testResults.recordPass("execSqlAsyncFuture");
    } catch (const DrogonDbException &e) {
        testResults.recordFail("execSqlAsyncFuture", e.base().what());
    }

    // ============================================
    // Test 20: Batch Operations
    // ============================================
    LOG_INFO << "\n[Test 20] Testing Batch Operations...";
    *clientPtr << "DROP TABLE IF EXISTS test_batch" << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    *clientPtr << "CREATE TABLE test_batch (id INTEGER PRIMARY KEY, value INTEGER)"
               << Mode::Blocking >>
        [](const Result &r) {} >>
        [](const DrogonDbException &e) {};

    // Insert multiple rows
    for (int i = 1; i <= 10; i++) {
        *clientPtr << "INSERT INTO test_batch (id, value) VALUES (?, ?)"
                   << i << (i * 10)
                   << Mode::Blocking >>
            [](const Result &r) {} >>
            [](const DrogonDbException &e) {};
    }

    *clientPtr << "SELECT COUNT(*) as cnt FROM test_batch" >>
        [](const Result &r) {
            assert(r[0]["cnt"].as<int>() == 10);
            testResults.recordPass("Batch insert operations");
        } >>
        [](const DrogonDbException &e) {
            testResults.recordFail("Batch insert operations", e.base().what());
        };

    // Wait for all async operations to complete
    std::this_thread::sleep_for(2s);

    // Print test summary
    testResults.printSummary();

    LOG_INFO << "\n==========================================";
    LOG_INFO << "DuckDB Comprehensive Test Suite Complete";
    LOG_INFO << "==========================================\n";

    return (testResults.failed == 0) ? 0 : 1;
}
