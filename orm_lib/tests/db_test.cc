/**
 *
 *  db_test.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 *  Drogon database test program
 *
 */
#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <stdlib.h>

#include "mysql/Users.h"
#include "postgresql/Users.h"
#include "sqlite3/Users.h"

using namespace std::chrono_literals;
using namespace drogon::orm;

#define RESET "\033[0m"
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */

constexpr int postgre_tests = 42;
constexpr int mysql_tests = 44;
constexpr int sqlite_tests = 47;

int test_count = 0;
int counter = 0;
int gLoops = 1;
std::promise<int> pro;
auto globalf = pro.get_future();

using namespace std::chrono_literals;

int get_test_count();

void addCount(int &count, std::promise<int> &pro)
{
    ++count;
    // LOG_DEBUG << count;
    if (count == test_count)
    {
        pro.set_value(1);
    }
}

void testOutput(bool isGood, const std::string &testMessage)
{
    if (isGood)
    {
        std::cout << GREEN << counter + 1 << ".\t" << testMessage << "\t\tOK\n";
        std::cout << RESET;
        addCount(counter, pro);
    }
    else
    {
        std::cout << RED << testMessage << "\t\tBAD\n";
        std::cout << RESET;
        exit(1);
    }
}

void doPostgreTest(const drogon::orm::DbClientPtr &clientPtr)
{
    // Prepare the test environment
    *clientPtr << "DROP TABLE IF EXISTS USERS" >> [](const Result &r) {
        testOutput(true, "postgresql - Prepare the test environment(0)");
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - Prepare the test environment(0)");
    };
    *clientPtr << "CREATE TABLE users \
        (\
            user_id character varying(32),\
            user_name character varying(64),\
            password character varying(64),\
            org_name character varying(20),\
            signature character varying(50),\
            avatar_id character varying(32),\
            id serial PRIMARY KEY,\
            salt character varying(20),\
            admin boolean DEFAULT false,\
            CONSTRAINT user_id_org UNIQUE(user_id, org_name)\
        )" >>
        [](const Result &r) {
            testOutput(true, "postgresql - Prepare the test environment(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "postgresql - Prepare the test environment(1)");
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values($1,$2,$3,$4) returning *"
               << "pg"
               << "postgresql"
               << "123"
               << "default" >>
        [](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            testOutput(r[0]["id"].as<int64_t>() == 1,
                       "postgresql - DbClient streaming-type interface(0)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(0)");
        };
    /// 1.2 insert,blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values($1,$2,$3,$4) returning *"
               << "pg1"
               << "postgresql1"
               << "123"
               << "default" << Mode::Blocking >>
        [](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            testOutput(r[0]["id"].as<int64_t>() == 2,
                       "postgresql - DbClient streaming-type interface(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(1)");
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "postgresql - DbClient streaming-type interface(2)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(2)");
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "postgresql - DbClient streaming-type interface(3)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(3)");
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count](bool isNull,
                 const std::string &name,
                 std::string &&user_id,
                 int id) {
            if (!isNull)
                ++count;
            else
            {
                testOutput(count == 2,
                           "postgresql - DbClient streaming-type interface(4)");
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(4)");
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = $1" << 1 >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "postgresql - DbClient streaming-type interface(5)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(5)");
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "postgresql - DbClient streaming-type interface(6)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(6)");
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "postgresql - DbClient streaming-type interface(7)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(7)");
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = $1, user_name = $2 where user_id "
                  "= $3 and user_name = $4"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "postgresql - DbClient streaming-type interface(8)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(8)");
        };
    /// 1.10 clean up
    *clientPtr << "truncate table users restart identity" >>
        [](const Result &r) {
            testOutput(true,
                       "postgresql - DbClient streaming-type interface(9)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient streaming-type interface(9)");
        };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values($1,$2,$3,$4) returning *",
        [](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            testOutput(r[0]["id"].as<int64_t>() == 1,
                       "postgresql - DbClient asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(0)");
        },
        "pg",
        "postgresql",
        "123",
        "default");
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values($1,$2,$3,$4)",
        [](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            testOutput(r.affectedRows() == 1,
                       "postgresql - DbClient asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(1)");
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "postgresql - DbClient asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(2)");
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = $1",
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "postgresql - DbClient asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(3)");
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = $1 and user_name = $2",
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "postgresql - DbClient asynchronous interface(4)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(4)");
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = $1 and user_name = $2",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "postgresql - DbClient asynchronous interface(5)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(5)");
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = $1, user_name = $2 where user_id "
        "= $3 and user_name = $4",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "postgresql - DbClient asynchronous interface(6)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(6)");
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 clean up
    clientPtr->execSqlAsync(
        "truncate table users restart identity",
        [](const Result &r) {
            testOutput(true, "postgresql - DbClient asynchronous interface(7)");
        },
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - DbClient asynchronous interface(7)");
        });

    /// Test synchronous method
    /// 3.1 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values($1,$2,$3,$4) returning *",
            "pg",
            "postgresql",
            "123",
            "default");
        testOutput(r[0]["id"].as<int64_t>() == 1,
                   "postgresql - DbClient synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient asynchronous interface(0)");
    }
    /// 3.2 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values($1,$2,$3,$4)",
            "pg1",
            "postgresql1",
            "123",
            "default");
        testOutput(r.affectedRows() == 1,
                   "postgresql - DbClient synchronous interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient asynchronous interface(1)");
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg1",
            "postgresql1");
        testOutput(r.size() == 1,
                   "postgresql - DbClient synchronous interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient asynchronous interface(2)");
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0,
                   "postgresql - DbClient synchronous interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient asynchronous interface(3)");
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name='1234'",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0,
                   "postgresql - DbClient synchronous interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "postgresql - DbClient asynchronous interface(4)");
    }
    /// 3.6 clean up
    try
    {
        auto r =
            clientPtr->execSqlSync("truncate table users restart identity");
        testOutput(true, "postgresql - DbClient synchronous interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient asynchronous interface(5)");
    }
    /// Test future interface
    /// 4.1 insert
    auto f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values($1,$2,$3,$4) returning *",
        "pg",
        "postgresql",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r[0]["id"].as<int64_t>() == 1,
                   "postgresql - DbClient future interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient future interface(0)");
    }
    /// 4.2 insert
    f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values($1,$2,$3,$4)",
        "pg1",
        "postgresql1",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r.affectedRows() == 1,
                   "postgresql - DbClient future interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient future interface(1)");
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 1, "postgresql - DbClient future interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient future interface(2)");
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "postgresql - DbClient future interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient future interface(3)");
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "postgresql - DbClient future interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "postgresql - DbClient future interface(4)");
    }
    /// 4.6 clean up
    f = clientPtr->execSqlAsyncFuture("truncate table users restart identity");
    try
    {
        auto r = f.get();
        testOutput(true, "postgresql - DbClient future interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - DbClient future interface(5)");
    }

    /// 5 Test Result and Row exception throwing
    // 5.1 query for none and try to access
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg111",
            "postgresql1");
        r.at(0);
        testOutput(false, "postgresql - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "postgresql -  Result throwing exceptions(0)");
    }
    // 5.2 insert one just for setup
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values($1,$2,$3,$4) returning *",
            "pg",
            "postgresql",
            "123",
            "default");
        testOutput(r[0]["id"].as<int64_t>() == 1,
                   "postgresql - Row throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - Row throwing exceptions(0)");
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        testOutput(false, "postgresql - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "postgresql -  Row throwing exceptions(1)");
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        testOutput(false, "postgresql - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "postgresql -  Row throwing exceptions(2)");
    }

    // 5.5 cleanup
    try
    {
        auto r =
            clientPtr->execSqlSync("truncate table users restart identity");
        testOutput(true, "postgresql - Row throwing exceptions(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql -  Row throwing exceptions(3)");
    }

    /// Test ORM mapper
    /// 6.1 insert, noneblocking
    using namespace drogon_model::postgres;
    drogon::orm::Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 1,
                       "postgresql - ORM mapper asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - ORM mapper asynchronous interface(0)");
        });
    /// 6.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 2,
                       "postgresql - ORM mapper asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - ORM mapper asynchronous interface(1)");
        });
    /// 6.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [](std::vector<Users> users) {
            testOutput(users.size() == 1,
                       "postgresql - ORM mapper asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - ORM mapper asynchronous interface(2)");
        });
    /// 6.3.5 count
    mapper.count(
        drogon::orm::Criteria(Users::Cols::_id, CompareOperator::EQ, 2020),
        [](const size_t c) {
            testOutput(c == 0,
                       "postgresql - ORM mapper asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false,
                       "postgresql - ORM mapper asynchronous interface(3)");
        });
    /// 6.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(2);
        testOutput(true, "postgresql - ORM mapper synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "postgresql - ORM mapper synchronous interface(0)");
    }
}

void doMysqlTest(const drogon::orm::DbClientPtr &clientPtr)
{
    // Prepare the test environment
    *clientPtr << "CREATE DATABASE IF NOT EXISTS drogonTestMysql" >>
        [](const Result &r) {
            testOutput(true, "mysql - Prepare the test environment(0)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - Prepare the test environment(0)");
        };
    *clientPtr << "USE drogonTestMysql" >> [](const Result &r) {
        testOutput(true, "mysql - Prepare the test environment(0)");
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - Prepare the test environment(0)");
    };
    // mysql is case sensitive
    *clientPtr << "DROP TABLE IF EXISTS users" >> [](const Result &r) {
        testOutput(true, "mysql - Prepare the test environment(1)");
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - Prepare the test environment(1)");
    };
    *clientPtr << "CREATE TABLE users \
        (\
            id int(11) auto_increment PRIMARY KEY,\
            user_id varchar(32),\
            user_name varchar(64),\
            password varchar(64),\
            org_name varchar(20),\
            signature varchar(50),\
            avatar_id varchar(32),\
            salt character varying(20),\
            admin boolean DEFAULT false,\
            CONSTRAINT user_id_org UNIQUE(user_id, org_name)\
        )" >>
        [](const Result &r) {
            testOutput(true, "mysql - Prepare the test environment(2)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - Prepare the test environment(2)");
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values(?,?,?,?)"
               << "pg"
               << "postgresql"
               << "123"
               << "default" >>
        [](const Result &r) {
            testOutput(r.insertId() == 1,
                       "mysql - DbClient streaming-type interface(0)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(0)");
        };
    /// 1.2 insert,blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values(?,?,?,?)"
               << "pg1"
               << "postgresql1"
               << "123"
               << "default" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.insertId() == 2,
                       "mysql - DbClient streaming-type interface(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(1)");
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "mysql - DbClient streaming-type interface(2)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(2)");
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "mysql - DbClient streaming-type interface(3)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(3)");
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count](bool isNull,
                 const std::string &name,
                 std::string &&user_id,
                 int id) {
            if (!isNull)
                ++count;
            else
            {
                testOutput(count == 2,
                           "mysql - DbClient streaming-type interface(4)");
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(4)");
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = ?" << 1 >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "mysql - DbClient streaming-type interface(5)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(5)");
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "mysql - DbClient streaming-type interface(6)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(6)");
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "mysql - DbClient streaming-type interface(7)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(7)");
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = ?, user_name = ? where user_id "
                  "= ? and user_name = ?"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "mysql - DbClient streaming-type interface(8)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient streaming-type interface(8)");
        };
    /// 1.10 truncate
    *clientPtr << "truncate table users" >> [](const Result &r) {
        testOutput(true, "mysql - DbClient streaming-type interface(9)");
    } >> [](const DrogonDbException &e) {
        std::cerr << "error:" << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient streaming-type interface(9)");
    };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values(?,?,?,?)",
        [](const Result &r) {
            testOutput(r.insertId() != 0,
                       "mysql - DbClient asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(0)");
        },
        "pg",
        "postgresql",
        "123",
        "default");
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values(?,?,?,?)",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "mysql - DbClient asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(1)");
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "mysql - DbClient asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(2)");
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = ?",
        [](const Result &r) {
            // std::cout << r.size() << "\n";
            testOutput(r.size() == 1,
                       "mysql - DbClient asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(3)");
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "mysql - DbClient asynchronous interface(4)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(4)");
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "mysql - DbClient asynchronous interface(5)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(5)");
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = ?, user_name = ? where user_id "
        "= ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "mysql - DbClient asynchronous interface(6)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(6)");
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 truncate
    clientPtr->execSqlAsync(
        "truncate table users",
        [](const Result &r) {
            testOutput(true, "mysql - DbClient asynchronous interface(7)");
        },
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false, "mysql - DbClient asynchronous interface(7)");
        });

    /// Test synchronous method
    /// 3.1 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default");
        // std::cout << r.insertId();
        testOutput(r.insertId() == 1,
                   "mysql - DbClient synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient asynchronous interface(0)");
    }
    /// 3.2 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg1",
            "postgresql1",
            "123",
            "default");
        testOutput(r.affectedRows() == 1,
                   "mysql - DbClient synchronous interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient asynchronous interface(1)");
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg1",
            "postgresql1");
        testOutput(r.size() == 1, "mysql - DbClient synchronous interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient asynchronous interface(2)");
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0, "mysql - DbClient synchronous interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient asynchronous interface(3)");
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name='1234'",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0, "mysql - DbClient synchronous interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql - DbClient asynchronous interface(4)");
    }
    /// 3.6 truncate
    try
    {
        auto r = clientPtr->execSqlSync("truncate table users");
        testOutput(true, "mysql - DbClient synchronous interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql - DbClient asynchronous interface(5)");
    }
    /// Test future interface
    /// 4.1 insert
    auto f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values(?,?,?,?) ",
        "pg",
        "postgresql",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r.insertId() == 1, "mysql - DbClient future interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient future interface(0)");
    }
    /// 4.2 insert
    f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values(?,?,?,?)",
        "pg1",
        "postgresql1",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r.affectedRows() == 1,
                   "mysql - DbClient future interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient future interface(1)");
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 1, "mysql - DbClient future interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient future interface(2)");
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "mysql - DbClient future interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient future interface(3)");
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "mysql - DbClient future interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql - DbClient future interface(4)");
    }
    /// 4.6 truncate
    f = clientPtr->execSqlAsyncFuture("truncate table users");
    try
    {
        auto r = f.get();
        testOutput(true, "mysql - DbClient future interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - DbClient future interface(5)");
    }

    /// 5 Test Result and Row exception throwing
    // 5.1 query for none and try to access
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        r.at(0);
        testOutput(false, "mysql - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql -  Result throwing exceptions(0)");
    }
    // 5.2 insert one just for setup
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default");
        testOutput(r.insertId() == 1, "mysql - Row throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - Row throwing exceptions(0)");
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        testOutput(false, "mysql - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql -  Row throwing exceptions(1)");
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        testOutput(false, "mysql - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "mysql -  Row throwing exceptions(2)");
    }

    // 5.5 cleanup
    try
    {
        auto r = clientPtr->execSqlSync("truncate table users");
        testOutput(true, "mysql - Row throwing exceptions(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql -  Row throwing exceptions(3)");
    }

    /// Test ORM mapper
    /// 6.1 insert, noneblocking
    using namespace drogon_model::drogonTestMysql;
    drogon::orm::Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 1,
                       "mysql - ORM mapper asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - ORM mapper asynchronous interface(0)");
        });
    /// 6.1.5 count
    mapper.count(
        drogon::orm::Criteria(Users::Cols::_id, CompareOperator::EQ, 1),
        [](const size_t c) {
            testOutput(c == 1, "mysql - ORM mapper asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - ORM mapper asynchronous interface(1)");
        });
    /// 6.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 2,
                       "mysql - ORM mapper asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - ORM mapper asynchronous interface(2)");
        });
    /// 6.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [](std::vector<Users> users) {
            testOutput(users.size() == 1,
                       "mysql - ORM mapper asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "mysql - ORM mapper asynchronous interface(3)");
        });

    /// 6.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(1);
        testOutput(true, "mysql - ORM mapper synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "mysql - ORM mapper synchronous interface(0)");
    }
}

void doSqliteTest(const drogon::orm::DbClientPtr &clientPtr)
{
    // Prepare the test environment
    *clientPtr << "DROP TABLE IF EXISTS users" >> [](const Result &r) {
        testOutput(true, "sqlite3 - Prepare the test environment(0)");
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - Prepare the test environment(0)");
    };
    *clientPtr << "CREATE TABLE users \
        (\
            id INTEGER PRIMARY KEY autoincrement,\
            user_id varchar(32),\
            user_name varchar(64),\
            password varchar(64),\
            org_name varchar(20),\
            signature varchar(50),\
            avatar_id varchar(32),\
            salt character varchar(20),\
            admin boolean DEFAULT false,\
            CONSTRAINT user_id_org UNIQUE(user_id, org_name)\
        )" >>
        [](const Result &r) {
            testOutput(true, "sqlite3 - Prepare the test environment(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - Prepare the test environment(1)");
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values(?,?,?,?)"
               << "pg"
               << "postgresql"
               << "123"
               << "default" >>
        [](const Result &r) {
            testOutput(r.insertId() == 1,
                       "sqlite3 - DbClient streaming-type interface(0)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(0)");
        };
    /// 1.2 insert,blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values(?,?,?,?)"
               << "pg1"
               << "postgresql1"
               << "123"
               << "default" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.insertId() == 2,
                       "sqlite3 - DbClient streaming-type interface(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(1)");
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "sqlite3 - DbClient streaming-type interface(2)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(2)");
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "sqlite3 - DbClient streaming-type interface(3)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(3)");
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count](bool isNull,
                 const std::string &name,
                 std::string &&user_id,
                 int id) {
            if (!isNull)
                ++count;
            else
            {
                testOutput(count == 2,
                           "sqlite3 - DbClient streaming-type interface(4)");
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(4)");
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = ?" << 1 >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "sqlite3 - DbClient streaming-type interface(5)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(5)");
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "sqlite3 - DbClient streaming-type interface(6)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(6)");
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "sqlite3 - DbClient streaming-type interface(7)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(7)");
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = ?, user_name = ? where user_id "
                  "= ? and user_name = ?"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "sqlite3 - DbClient streaming-type interface(8)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient streaming-type interface(8)");
        };
    /// 1.10 clean up
    *clientPtr << "delete from users" >> [](const Result &r) {
        testOutput(true, "sqlite3 - DbClient streaming-type interface(9.1)");
    } >> [](const DrogonDbException &e) {
        std::cerr << "error:" << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient streaming-type interface(9.1)");
    };
    *clientPtr << "UPDATE sqlite_sequence SET seq = 0" >> [](const Result &r) {
        testOutput(true, "sqlite3 - DbClient streaming-type interface(9.2)");
    } >> [](const DrogonDbException &e) {
        std::cerr << "error:" << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient streaming-type interface(9.2)");
    };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values(?,?,?,?)",
        [](const Result &r) {
            testOutput(r.insertId() == 1,
                       "sqlite3 - DbClient asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(0)");
        },
        "pg",
        "postgresql",
        "123",
        "default");
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values(?,?,?,?)",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "sqlite3 - DbClient asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(1)");
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [](const Result &r) {
            testOutput(r.size() == 2,
                       "sqlite3 - DbClient asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(2)");
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = ?",
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "sqlite3 - DbClient asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(3)");
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.size() == 1,
                       "sqlite3 - DbClient asynchronous interface(4)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(4)");
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "sqlite3 - DbClient asynchronous interface(5)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(5)");
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = ?, user_name = ? where user_id "
        "= ? and user_name = ?",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "sqlite3 - DbClient asynchronous interface(6)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(6)");
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 clean up
    clientPtr->execSqlAsync(
        "delete from users",
        [](const Result &r) {
            testOutput(true, "sqlite3 - DbClient asynchronous interface(7.1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(7.1)");
        });
    clientPtr->execSqlAsync(
        "UPDATE sqlite_sequence SET seq = 0",
        [](const Result &r) {
            testOutput(true, "sqlite3 - DbClient asynchronous interface(7.2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - DbClient asynchronous interface(7.2)");
        });

    /// Test synchronous method
    /// 3.1 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default");
        testOutput(r.insertId() == 1,
                   "sqlite3 - DbClient synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient asynchronous interface(0)");
    }
    /// 3.2 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg1",
            "postgresql1",
            "123",
            "default");
        testOutput(r.affectedRows() == 1,
                   "sqlite3 - DbClient synchronous interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient asynchronous interface(1)");
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg1",
            "postgresql1");
        testOutput(r.size() == 1,
                   "sqlite3 - DbClient synchronous interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient asynchronous interface(2)");
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0,
                   "sqlite3 - DbClient synchronous interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient asynchronous interface(3)");
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name='1234'",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0,
                   "sqlite3 - DbClient synchronous interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 - DbClient asynchronous interface(4)");
    }
    /// 3.6 clean up
    try
    {
        auto r = clientPtr->execSqlSync("delete from users");
        testOutput(true, "sqlite3 - DbClient synchronous interface(5.1)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 - DbClient asynchronous interface(5.1)");
    }
    try
    {
        auto r = clientPtr->execSqlSync("UPDATE sqlite_sequence SET seq = 0");
        testOutput(true, "sqlite3 - DbClient synchronous interface(5.2)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 - DbClient asynchronous interface(5.2)");
    }

    /// Test future interface
    /// 4.1 insert
    auto f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values(?,?,?,?) ",
        "pg",
        "postgresql",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r.insertId() == 1, "sqlite3 - DbClient future interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(0)");
    }
    /// 4.2 insert
    f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name) "
        "values(?,?,?,?)",
        "pg1",
        "postgresql1",
        "123",
        "default");
    try
    {
        auto r = f.get();
        testOutput(r.affectedRows() == 1,
                   "sqlite3 - DbClient future interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(1)");
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 1, "sqlite3 - DbClient future interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(2)");
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "sqlite3 - DbClient future interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(3)");
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "sqlite3 - DbClient future interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 - DbClient future interface(4)");
    }
    /// 4.6 clean up
    f = clientPtr->execSqlAsyncFuture("delete from users");
    try
    {
        auto r = f.get();
        testOutput(true, "sqlite3 - DbClient future interface(5.1)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(5.1)");
    }
    f = clientPtr->execSqlAsyncFuture("UPDATE sqlite_sequence SET seq = 0");
    try
    {
        auto r = f.get();
        testOutput(true, "sqlite3 - DbClient future interface(5.2)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - DbClient future interface(5.2)");
    }

    /// 5 Test Result and Row exception throwing
    // 5.1 query for none and try to access
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        r.at(0);
        testOutput(false, "sqlite3 - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 -  Result throwing exceptions(0)");
    }
    // 5.2 insert one just for setup
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  (user_id,user_name,password,org_name) "
            "values(?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default");
        testOutput(r.insertId() == 1, "sqlite3 - Row throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - Row throwing exceptions(0)");
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        testOutput(false, "sqlite3 - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 -  Row throwing exceptions(1)");
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        testOutput(false, "sqlite3 - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(true, "sqlite3 -  Row throwing exceptions(2)");
    }

    // 5.5 cleanup
    try
    {
        auto r = clientPtr->execSqlSync("delete from users");
        testOutput(true, "sqlite3 - Row throwing exceptions(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 -  Row throwing exceptions(3)");
    }
    try
    {
        auto r = clientPtr->execSqlSync("UPDATE sqlite_sequence SET seq = 0");
        testOutput(true, "sqlite3 - Row throwing exceptions(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 -  Row throwing exceptions(3)");
    }

    /// Test ORM mapper TODO
    /// 5.1 insert, noneblocking
    using namespace drogon_model::sqlite3;
    drogon::orm::Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 1,
                       "sqlite3 - ORM mapper asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - ORM mapper asynchronous interface(0)");
        });
    /// 5.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 2,
                       "sqlite3 - ORM mapper asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - ORM mapper asynchronous interface(1)");
        });
    /// 5.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [](std::vector<Users> users) {
            testOutput(users.size() == 1,
                       "sqlite3 - ORM mapper asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - ORM mapper asynchronous interface(2)");
        });
    /// 5.3.5 count
    mapper.count(
        drogon::orm::Criteria(Users::Cols::_id, CompareOperator::EQ, 2),
        [](const size_t c) {
            testOutput(c == 1,
                       "sqlite3 - ORM mapper asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "sqlite3 - ORM mapper asynchronous interface(3)");
        });
    /// 5.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(1);
        testOutput(true, "sqlite3 - ORM mapper synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "sqlite3 - ORM mapper synchronous interface(0)");
    }
}

int main(int argc, char *argv[])
{
    trantor::Logger::setLogLevel(trantor::Logger::kDebug);
#if USE_POSTGRESQL
    auto postgre_client = DbClient::newPgClient(
        "host=127.0.0.1 port=5432 dbname=postgres user=postgres "
        "client_encoding=utf8",
        1);
    while (!postgre_client->hasAvailableConnections())
    {
        std::this_thread::sleep_for(1s);
    }
#endif
#if USE_MYSQL
    auto mysql_client = DbClient::newMysqlClient(
        "host=localhost port=3306 user=root client_encoding=utf8mb4", 1);
    while (!mysql_client->hasAvailableConnections())
    {
        std::this_thread::sleep_for(1s);
    }
#endif
#if USE_SQLITE3
    auto sqlite_client = DbClient::newSqlite3Client("filename=:memory:", 1);
    while (!sqlite_client->hasAvailableConnections())
    {
        std::this_thread::sleep_for(1s);
    }
#endif
    LOG_DEBUG << "start!";
    std::this_thread::sleep_for(1s);
    if (argc == 2)
    {
        gLoops = atoi(argv[1]);
    }
    test_count = get_test_count();
    for (int i = 0; i < gLoops; ++i)
    {
#if USE_POSTGRESQL
        doPostgreTest(postgre_client);
#endif
#if USE_MYSQL
        doMysqlTest(mysql_client);
#endif
#if USE_SQLITE3
        doSqliteTest(sqlite_client);
#endif
    }
    globalf.get();
    std::this_thread::sleep_for(0.008s);
    return 0;
}

int get_test_count()
{
    int test_count = 0;
#if USE_POSTGRESQL
    test_count += postgre_tests * gLoops;
#endif
#if USE_MYSQL
    test_count += mysql_tests * gLoops;
#endif
#if USE_SQLITE3
    test_count += sqlite_tests * gLoops;
#endif
    return test_count;
}
