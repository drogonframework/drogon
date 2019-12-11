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
#include "Users.h"
#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <stdlib.h>

using namespace drogon::orm;
using namespace drogon_model::postgres;

#define RESET "\033[0m"
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */
#define TEST_COUNT 36

int counter = 0;
int gLoops = 1;
std::promise<int> pro;
auto globalf = pro.get_future();

using namespace std::chrono_literals;
void addCount(int &count, std::promise<int> &pro)
{
    ++count;
    // LOG_DEBUG << count;
    if (count == TEST_COUNT * gLoops)
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

void doTest(const drogon::orm::DbClientPtr &clientPtr)
{
    // Prepare the test environment
    *clientPtr << "DROP TABLE IF EXISTS USERS" >> [](const Result &r) {
        testOutput(true, "Prepare the test environment(0)");
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "Prepare the test environment(0)");
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
            testOutput(true, "Prepare the test environment(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "Prepare the test environment(1)");
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
                       "DbClient streaming-type interface(0)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(0)");
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
                       "DbClient streaming-type interface(1)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(1)");
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [](const Result &r) {
            testOutput(r.size() == 2, "DbClient streaming-type interface(2)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(2)");
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [](const Result &r) {
            testOutput(r.size() == 2, "DbClient streaming-type interface(3)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(3)");
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count](bool isNull,
                 const std::string &name,
                 std::string user_id,
                 int id) {
            if (!isNull)
                ++count;
            else
            {
                testOutput(count == 2, "DbClient streaming-type interface(4)");
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(4)");
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = $1" << 1 >>
        [](const Result &r) {
            testOutput(r.size() == 1, "DbClient streaming-type interface(5)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(5)");
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.size() == 1, "DbClient streaming-type interface(6)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(6)");
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "DbClient streaming-type interface(7)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(7)");
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
                       "DbClient streaming-type interface(8)");
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient streaming-type interface(8)");
        };
    /// 1.10 truncate
    *clientPtr << "truncate table users" >> [](const Result &r) {
        testOutput(true, "DbClient streaming-type interface(9)");
    } >> [](const DrogonDbException &e) {
        std::cerr << "error:" << e.base().what() << std::endl;
        testOutput(false, "DbClient streaming-type interface(9)");
    };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users \
        (user_id,user_name,password,org_name) \
        values($1,$2,$3,$4) returning *",
        [](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            testOutput(r[0]["id"].as<int64_t>() == 3,
                       "DbClient asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(0)");
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
                       "DbClient asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(1)");
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [](const Result &r) {
            testOutput(r.size() == 2, "DbClient asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(2)");
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = $1",
        [](const Result &r) {
            testOutput(r.size() == 0, "DbClient asynchronous interface(3)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(3)");
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = $1 and user_name = $2",
        [](const Result &r) {
            testOutput(r.size() == 1, "DbClient asynchronous interface(4)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(4)");
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = $1 and user_name = $2",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "DbClient asynchronous interface(5)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(5)");
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = $1, user_name = $2 where user_id "
        "= $3 and user_name = $4",
        [](const Result &r) {
            testOutput(r.affectedRows() == 1,
                       "DbClient asynchronous interface(6)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(6)");
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 truncate
    clientPtr->execSqlAsync(
        "truncate table users",
        [](const Result &r) {
            testOutput(true, "DbClient asynchronous interface(7)");
        },
        [](const DrogonDbException &e) {
            std::cerr << "error:" << e.base().what() << std::endl;
            testOutput(false, "DbClient asynchronous interface(7)");
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
        testOutput(r[0]["id"].as<int64_t>() == 5,
                   "DbClient synchronous interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient asynchronous interface(0)");
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
        testOutput(r.affectedRows() == 1, "DbClient synchronous interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient asynchronous interface(1)");
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg1",
            "postgresql1");
        testOutput(r.size() == 1, "DbClient synchronous interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient asynchronous interface(2)");
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0, "DbClient synchronous interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient asynchronous interface(3)");
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name='1234'",
            "pg111",
            "postgresql1");
        testOutput(r.size() == 0, "DbClient synchronous interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "DbClient asynchronous interface(4)");
    }
    /// 3.6 truncate
    try
    {
        auto r = clientPtr->execSqlSync("truncate table users");
        testOutput(true, "DbClient synchronous interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "DbClient asynchronous interface(5)");
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
        testOutput(r[0]["id"].as<int64_t>() == 7,
                   "DbClient future interface(0)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient future interface(0)");
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
        testOutput(r.affectedRows() == 1, "DbClient future interface(1)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient future interface(1)");
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 1, "DbClient future interface(2)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient future interface(2)");
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "DbClient future interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "DbClient future interface(3)");
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        testOutput(r.size() == 0, "DbClient future interface(4)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "DbClient future interface(4)");
    }
    /// 4.6 truncate
    f = clientPtr->execSqlAsyncFuture("truncate table users");
    try
    {
        auto r = f.get();
        testOutput(true, "DbClient future interface(5)");
    }
    catch (const DrogonDbException &e)
    {
        // std::cerr << e.base().what() << std::endl;
        testOutput(true, "DbClient future interface(5)");
    }
    /// Test ORM mapper
    /// 5.1 insert, noneblocking
    drogon::orm::Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 9,
                       "ORM mapper asynchronous interface(0)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "ORM mapper asynchronous interface(0)");
        });
    /// 5.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [](Users ret) {
            testOutput(ret.getPrimaryKey() == 10,
                       "ORM mapper asynchronous interface(1)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "ORM mapper asynchronous interface(1)");
        });
    /// 5.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::IN,
                 std::vector<int32_t>{10, 200}),
        [](std::vector<Users> users) {
            testOutput(users.size() == 1,
                       "ORM mapper asynchronous interface(2)");
        },
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            testOutput(false, "ORM mapper asynchronous interface(2)");
        });

    /// 5.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(10);
        testOutput(true, "ORM mapper asynchronous interface(3)");
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << e.base().what() << std::endl;
        testOutput(false, "ORM mapper asynchronous interface(3)");
    }
}
int main(int argc, char *argv[])
{
    trantor::Logger::setLogLevel(trantor::Logger::DEBUG);
#if USE_POSTGRESQL
    auto clientPtr = DbClient::newPgClient(
        "host=127.0.0.1 port=5432 dbname=postgres user=postgres", 1);
#endif
    LOG_DEBUG << "start!";
    sleep(1);
    if (argc == 2)
    {
        gLoops = atoi(argv[1]);
    }
    for (int i = 0; i < gLoops; ++i)
    {
        doTest(clientPtr);
    }
    globalf.get();
    std::this_thread::sleep_for(0.008s);
    return 0;
}
