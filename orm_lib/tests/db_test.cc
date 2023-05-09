/**
 *
 *  @file db_test.cc
 *  @author An Tao
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
#define DROGON_TEST_MAIN
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>
#include <drogon/drogon_test.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/DbTypes.h>
#include <drogon/utils/string_view.h>
#include <drogon/orm/QueryBuilder.h>
#include <trantor/utils/Logger.h>

#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mysql/Users.h"
#include "postgresql/Users.h"
#include "sqlite3/Users.h"

using namespace std::chrono_literals;
using namespace drogon::orm;

void expFunction(const DrogonDbException &e)
{
}
#if USE_POSTGRESQL
DbClientPtr postgreClient;
DROGON_TEST(PostgreTest)
{
    auto &clientPtr = postgreClient;

    // Test bugfix #1588
    // PgBatchConnection.cc did not report error message to application
    try
    {
        clientPtr->execSqlSync("select * from t_not_exists");
    }
    catch (const DrogonDbException &e)
    {
        MANDATE(!std::string{e.base().what()}.empty());
    }

    // Prepare the test environment
    *clientPtr << "DROP TABLE IF EXISTS USERS" >> [TEST_CTX,
                                                   clientPtr](const Result &r) {
        SUCCESS();
        clientPtr->execSqlAsync(
            "select 1 as result",
            [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
            expFunction);
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("postgresql - Prepare the test environment(0) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE users "
                  "("
                  "    user_id character varying(32),"
                  "    user_name character varying(64),"
                  "    password character varying(64),"
                  "    org_name character varying(20),"
                  "    signature character varying(50),"
                  "    avatar_id character varying(32),"
                  "    id serial PRIMARY KEY,"
                  "    salt character varying(20),"
                  "    admin boolean DEFAULT false,"
                  "    CONSTRAINT user_id_org UNIQUE(user_id, org_name)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(1) what():",
                  e.base().what());
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values($1,$2,$3,$4) returning *"
               << "pg"
               << "postgresql"
               << "123"
               << "default" >>
        [TEST_CTX](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            MANDATE(r[0]["id"].as<int64_t>() == 1);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(0) what():",
                  e.base().what());
        };
    /// 1.2 insert,blocking
    *clientPtr
            << "insert into users (user_id,user_name,admin,password,org_name) "
               "values($1,$2,$3,$4,$5) returning *"
            << "pg1"
            << "postgresql1" << DefaultValue{} << "123"
            << "default" << Mode::Blocking >>
        [TEST_CTX](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            MANDATE(r[0]["id"].as<int64_t>() == 2);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(1) what():",
                  e.base().what());
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(2) what():",
                  e.base().what());
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(3) what():",
                  e.base().what());
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count, TEST_CTX](bool isNull,
                           const std::string &name,
                           std::string &&user_id,
                           int id) {
            if (!isNull)
                ++count;
            else
                MANDATE(count == 2);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(4) what():",
                  e.base().what());
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = $1" << 1 >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(5) what():",
                  e.base().what());
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(6) what():",
                  e.base().what());
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = $1 and user_name = $2"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(7) what():",
                  e.base().what());
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = $1, user_name = $2 where user_id "
                  "= $3 and user_name = $4"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(8) what():",
                  e.base().what());
        };
    /// 1.10 clean up
    *clientPtr << "truncate table users restart identity" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(9) what():",
                  e.base().what());
        };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name) "
        "values($1,$2,$3,$4) returning *",
        [TEST_CTX](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            MANDATE(r[0]["id"].as<int64_t>() == 1);
        },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(0) what():",
                  e.base().what());
        },
        "pg",
        "postgresql",
        "123",
        "default");
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name) "
        "values($1,$2,$3,$4)",
        [TEST_CTX](const Result &r) {
            // std::cout << "id=" << r[0]["id"].as<int64_t>() << std::endl;
            MANDATE(r.affectedRows() == 1);
        },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(1) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = $1",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(3) what():",
                  e.base().what());
        },
        1);
    /// 2.3 query, parameter binding (string_view)
    clientPtr->execSqlAsync(
        "select * from users where user_id = $1 and user_name = $2",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(4) what():",
                  e.base().what());
        },
        drogon::string_view("pg1"),
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = $1 and user_name = $2",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(5) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = $1, user_name = $2 where user_id "
        "= $3 and user_name = $4",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(6) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 clean up
    clientPtr->execSqlAsync(
        "truncate table users restart identity",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(7) what():",
                  e.base().what());
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
        MANDATE(r[0]["id"].as<int64_t>() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(0) what():",
              e.base().what());
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
        MANDATE(r.affectedRows() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(1) what():",
              e.base().what());
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg1",
            "postgresql1");
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(2) what():",
              e.base().what());
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name=$2",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(3) what():",
              e.base().what());
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=$1 and user_name='1234'",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 3.6 clean up
    try
    {
        auto r =
            clientPtr->execSqlSync("truncate table users restart identity");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(5) what():",
              e.base().what());
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
        MANDATE(r[0]["id"].as<int64_t>() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(0) what():",
              e.base().what());
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
        MANDATE(r.affectedRows() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(1) what():",
              e.base().what());
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(2) what():",
              e.base().what());
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name=$2",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(3) what():",
              e.base().what());
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=$1 and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 4.6 clean up
    f = clientPtr->execSqlAsyncFuture("truncate table users restart identity");
    try
    {
        auto r = f.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(5) what():",
              e.base().what());
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
        FAULT("postgresql - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
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
        MANDATE(r[0]["id"].as<int64_t>() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - Row throwing exceptions(0) what():",
              e.base().what());
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        FAULT("postgresql - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        FAULT("postgresql - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.5 cleanup
    try
    {
        auto r =
            clientPtr->execSqlSync("truncate table users restart identity");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - Row throwing exceptions(3) what():",
              e.base().what());
    }

    /// Test ORM mapper
    /// 6.1 insert, noneblocking
    using namespace drogon_model::postgres;
    Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(0) what():",
                  e.base().what());
        });

    /// 6.1.5 insert future
    user.setUserId("pg_future");
    auto fu = mapper.insertFuture(user);
    try
    {
        auto u = fu.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT(
            "postgresql - ORM mapper asynchronous future interface(0) what():",
            e.base().what());
    }

    /// 6.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 3); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(1) what():",
                  e.base().what());
        });
    /// 6.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 6.3.5 count
    mapper.count(
        Criteria(Users::Cols::_id, CompareOperator::EQ, 2020),
        [TEST_CTX](const size_t c) { MANDATE(c == 0); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(3) what():",
                  e.base().what());
        });
    /// 6.3.6 custom where query
    mapper.findBy(
        Criteria("id <@ int4range($?, null)"_sql, 3),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(4) what():",
                  e.base().what());
        });
    /// 6.3.7 pagination
    mapper.paginate(2, 1).findAll(
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - ORM mapper asynchronous interface(5) what():",
                  e.base().what());
        });
    /// 6.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(2);
        SUCCESS();
        Users newUser;
        newUser.setId(user.getValueOfId());
        newUser.setSalt("xxx");
        auto c = mapper.update(newUser);
        MANDATE(c == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM mapper synchronous interface(0) what():",
              e.base().what());
    }
    /// 6.5.1 update by criteria. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(2);
        SUCCESS();
        Users newUser;
        newUser.setId(user.getValueOfId());
        newUser.setSalt("xxx");
        newUser.setUserName(user.getValueOfUserName());
        auto c = mapper.update(newUser);
        MANDATE(c == 1);
        c = mapper.updateBy({Users::Cols::_avatar_id, Users::Cols::_salt},
                            Criteria(Users::Cols::_user_id,
                                     CompareOperator::EQ,
                                     "pg"),
                            "avatar of pg",
                            "salt of pg");
        MANDATE(c == 1);
        c = mapper.updateBy({Users::Cols::_avatar_id, Users::Cols::_salt},
                            Criteria(Users::Cols::_user_id,
                                     CompareOperator::EQ,
                                     "none"),
                            "avatar of none",
                            "salt of none");
        MANDATE(c == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM mapper synchronous interface(1) what():",
              e.base().what());
    }

    /// 6.5.2 update by criteria. asynchronous
    {
        std::vector<std::string> cols{Users::Cols::_avatar_id,
                                      Users::Cols::_salt};
        mapper.updateBy(
            cols,
            [TEST_CTX](const size_t c) { MANDATE(c == 1); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper asynchronous updateBy "
                    "interface(0) "
                    "what():",
                    e.base().what());
            },
            Criteria(Users::Cols::_user_id, CompareOperator::EQ, "pg"),
            "avatar of pg",
            "salt of pg");
    }

    /// Test ORM QueryBuilder
    /// execSync
    try
    {
        const std::vector<Users> users =
            QueryBuilder<Users>{}.from("users").selectAll().execSync(clientPtr);
        MANDATE(users.size() == 3);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM QueryBuilder synchronous interface(0) what():",
              e.base().what());
    }
    try
    {
        const Result users =
            QueryBuilder<Users>{}.from("users").select("id").execSync(
                clientPtr);
        MANDATE(users.size() == 3);
        for (const Row &u : users)
        {
            MANDATE(!u["id"].isNull());
        }
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM QueryBuilder synchronous interface(1) what():",
              e.base().what());
    }
    try
    {
        const Users user = QueryBuilder<Users>{}
                               .from("users")
                               .selectAll()
                               .eq("id", "3")
                               .limit(1)
                               .single()
                               .order("id", false)
                               .execSync(clientPtr);
        MANDATE(user.getPrimaryKey() == 3);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM QueryBuilder synchronous interface(2) what():",
              e.base().what());
    }
    try
    {
        const Row user = QueryBuilder<Users>{}
                             .from("users")
                             .select("id")
                             .limit(1)
                             .single()
                             .order("id", false)
                             .execSync(clientPtr);
        MANDATE(user["id"].as<int32_t>() == 3);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - ORM QueryBuilder synchronous interface(3) what():",
              e.base().what());
    }

    /// execAsyncFuture
    {
        std::future<std::vector<Users>> users =
            QueryBuilder<Users>{}.from("users").selectAll().execAsyncFuture(
                clientPtr);
        try
        {
            const std::vector<Users> r = users.get();
            MANDATE(r.size() == 3);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - ORM QueryBuilder asynchronous interface(0) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Result> users =
            QueryBuilder<Users>{}.from("users").select("id").execAsyncFuture(
                clientPtr);
        try
        {
            const Result r = users.get();
            MANDATE(r.size() == 3);
            for (const Row &u : r)
            {
                MANDATE(!u["id"].isNull());
            }
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - ORM QueryBuilder asynchronous interface(1) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Users> user = QueryBuilder<Users>{}
                                      .from("users")
                                      .selectAll()
                                      .eq("id", "3")
                                      .limit(1)
                                      .single()
                                      .order("id", false)
                                      .execAsyncFuture(clientPtr);
        try
        {
            const Users r = user.get();
            MANDATE(r.getPrimaryKey() == 3);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - ORM QueryBuilder asynchronous interface(2) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Row> users = QueryBuilder<Users>{}
                                     .from("users")
                                     .select("id")
                                     .limit(1)
                                     .single()
                                     .order("id", false)
                                     .execAsyncFuture(clientPtr);
        try
        {
            const Row r = users.get();
            MANDATE(r["id"].as<int32_t>() == 3);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - ORM QueryBuilder asynchronous interface(3) "
                "what():",
                e.base().what());
        }
    }

#ifdef __cpp_impl_coroutine
    auto coro_test = [clientPtr, TEST_CTX]() -> drogon::Task<> {
        /// 7 Test coroutines.
        /// This is by no means comprehensive. But coroutine API is esentially a
        /// wrapper arround callbacks. The purpose is to test the interface
        /// works 7.1 Basic queries
        try
        {
            auto result =
                co_await clientPtr->execSqlCoro("select * from users;");
            MANDATE(result.size() != 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - DbClient coroutine interface(0) what():",
                  e.base().what());
        }
        /// 7.2 Parameter binding
        try
        {
            auto result = co_await clientPtr->execSqlCoro(
                "select * from users where 1=$1;", 1);
            MANDATE(result.size() != 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - DbClient coroutine interface(1) what():",
                  e.base().what());
        }
        /// 7.3 CoroMapper
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto user = co_await mapper.findByPrimaryKey(2);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper coroutine interface(0) what():",
                  e.base().what());
        }
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto user = co_await mapper.findByPrimaryKey(314);
            FAULT("postgresql - ORM mapper coroutine interface(1)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto users = co_await mapper.findAll();
            auto count = co_await mapper.count();
            MANDATE(users.size() == count);
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto users = co_await mapper.orderBy(Users::Cols::_user_id)
                             .limit(2)
                             .findAll();
            MANDATE(users.size() == 2);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper coroutine interface(2) what():",
                  e.base().what());
        }
        // CoroMapper::update
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto user = co_await mapper.findByPrimaryKey(2);
            SUCCESS();
            Users newUser;
            newUser.setId(user.getValueOfId());
            newUser.setSalt("xxx");
            size_t c = co_await mapper.update(newUser);
            MANDATE(c == 1);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper coroutine interface(3) what():",
                  e.base().what());
        }
        // CoroMapper::updateBy
        try
        {
            CoroMapper<Users> mapper(clientPtr);
            auto awaiter = mapper.updateBy(
                {Users::Cols::_avatar_id, Users::Cols::_salt},
                Criteria(Users::Cols::_user_id, CompareOperator::EQ, "pg"),
                "avatar of pg",
                "salt of pg");
            size_t c = co_await awaiter;
            MANDATE(c == 1);
            // Can not compile if use co_await and initilizer_list together.
            // Seems to be a bug in gcc.
            std::vector<std::string> updateFields{Users::Cols::_avatar_id,
                                                  Users::Cols::_salt};
            c = co_await mapper.updateBy(updateFields,
                                         Criteria(Users::Cols::_user_id,
                                                  CompareOperator::EQ,
                                                  "none"),
                                         "avatar of none",
                                         "salt of none");
            MANDATE(c == 0);
            auto count = co_await mapper.count();
            // Use std::make_tuple to as an alternative to initializer_list
            c = co_await mapper.updateBy(
                std::make_tuple(Users::Cols::_avatar_id, Users::Cols::_salt),
                Criteria(),
                "avatar",
                "salt");
            MANDATE(c == count);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper coroutine interface(4) what():",
                  e.base().what());
        }
        /// 7.4 Transactions
        try
        {
            auto trans = co_await clientPtr->newTransactionCoro();
            auto result =
                co_await trans->execSqlCoro("select * from users where 1=$1;",
                                            1);
            MANDATE(result.size() != 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - DbClient coroutine transaction interface(0) "
                "what():",
                e.base().what());
        }
    };
    drogon::sync_wait(coro_test());

#endif
}
#endif

#if USE_MYSQL
DbClientPtr mysqlClient;
DROGON_TEST(MySQLTest)
{
    auto &clientPtr = mysqlClient;
    REQUIRE(clientPtr != nullptr);
    // Prepare the test environment
    *clientPtr << "CREATE DATABASE IF NOT EXISTS drogonTestMysql" >>
        [TEST_CTX, clientPtr](const Result &r) {
            SUCCESS();
            clientPtr->execSqlAsync(
                "select 1 as result",
                [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
                expFunction);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(0) what():",
                  e.base().what());
        };
    *clientPtr << "USE drogonTestMysql" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - Prepare the test environment(0) what():",
              e.base().what());
    };
    // mysql is case sensitive
    *clientPtr << "DROP TABLE IF EXISTS users" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - Prepare the test environment(1) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE users "
                  "("
                  "    id int(11) auto_increment PRIMARY KEY,"
                  "    user_id varchar(32),"
                  "    user_name varchar(64),"
                  "    password varchar(64),"
                  "    org_name varchar(20),"
                  "    signature varchar(50),"
                  "    avatar_id varchar(32),"
                  "    salt character varying(20),"
                  "    admin boolean DEFAULT false,"
                  "    CONSTRAINT user_id_org UNIQUE(user_id, org_name)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(2) what():",
                  e.base().what());
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr
            << "insert into users (user_id,user_name,password,org_name,admin) "
               "values(?,?,?,?,?)"
            << "pg"
            << "postgresql"
            << "123"
            << "default" << DefaultValue{} >>
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(0) what():",
                  e.base().what());
        };
    /// 1.2 insert,blocking
    *clientPtr << "insert into users (user_id,user_name,password,org_name) "
                  "values(?,?,?,?)"
               << "pg1"
               << "postgresql1"
               << "123"
               << "default" << Mode::Blocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() == 2); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(1) what():",
                  e.base().what());
        };
    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(2) what():",
                  e.base().what());
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(3) what():",
                  e.base().what());
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count, TEST_CTX](bool isNull,
                           const std::string &name,
                           std::string &&user_id,
                           int id) {
            if (!isNull)
                ++count;
            else
                MANDATE(count == 2);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(4) what():",
                  e.base().what());
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = ?" << 1 >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(5) what():",
                  e.base().what());
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(6) what():",
                  e.base().what());
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(7) what():",
                  e.base().what());
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = ?, user_name = ? where user_id "
                  "= ? and user_name = ?"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(8) what():",
                  e.base().what());
        };
    /// 1.10 truncate
    *clientPtr << "truncate table users" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - DbClient streaming-type interface(9) what():",
              e.base().what());
    };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name) "
        "values(?,?,?,?)",
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() != 0); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(0) what():",
                  e.base().what());
        },
        "pg",
        "postgresql",
        "123",
        "default");
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name) "
        "values(?,?,?,?)",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(1) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "123",
        "default");
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = ?",
        [TEST_CTX](const Result &r) {
            // std::cout << r.size() << "\n";
            MANDATE(r.size() == 1);
        },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(3) what():",
                  e.base().what());
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(4) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(5) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = ?, user_name = ? where user_id "
        "= ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(6) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 truncate
    clientPtr->execSqlAsync(
        "truncate table users",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(7) what():",
                  e.base().what());
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
        MANDATE(r.insertId() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient asynchronous interface(0) what():",
              e.base().what());
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
        MANDATE(r.affectedRows() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient asynchronous interface(1) what():",
              e.base().what());
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg1",
            "postgresql1");
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient asynchronous interface(2) what():",
              e.base().what());
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient asynchronous interface(3) what():",
              e.base().what());
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name='1234'",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 3.6 truncate
    try
    {
        auto r = clientPtr->execSqlSync("truncate table users");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
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
        MANDATE(r.insertId() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(0) what():", e.base().what());
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
        MANDATE(r.affectedRows() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(1) what():", e.base().what());
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(2) what():", e.base().what());
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(3) what():", e.base().what());
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 4.6 truncate
    f = clientPtr->execSqlAsyncFuture("truncate table users");
    try
    {
        auto r = f.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(5) what():", e.base().what());
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
        FAULT("mysql - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
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
        MANDATE(r.insertId() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - Row throwing exceptions(0) what():", e.base().what());
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        FAULT("mysql - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        FAULT("mysql - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.5 cleanup
    try
    {
        auto r = clientPtr->execSqlSync("truncate table users");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql -  Row throwing exceptions(3) what():", e.base().what());
    }

    /// Test ORM mapper
    /// 6.1 insert, noneblocking
    using namespace drogon_model::drogonTestMysql;
    Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - ORM mapper asynchronous interface(0) what():",
                  e.base().what());
        });
    /// 6.1.5 count
    mapper.count(
        Criteria(Users::Cols::_id, CompareOperator::EQ, 1),
        [TEST_CTX](const size_t c) { MANDATE(c == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - ORM mapper asynchronous interface(1) what():",
                  e.base().what());
        });
    /// 6.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 2); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - ORM mapper asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 6.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - ORM mapper asynchronous interface(3) what():",
                  e.base().what());
        });
    /// 6.3.6 custom where query
    mapper.findBy(
        Criteria("id between $? and $?"_sql, 2, 200),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - ORM mapper asynchronous interface(4) what():",
                  e.base().what());
        });
    /// 6.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(1);
        SUCCESS();
        Users newUser;
        newUser.setId(user.getValueOfId());
        newUser.setSalt("xxx");
        auto c = mapper.update(newUser);
        MANDATE(c == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM mapper synchronous interface(0) what():",
              e.base().what());
    }
    /// 6.5 update by criteria. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(2);
        SUCCESS();
        Users newUser;
        newUser.setId(user.getValueOfId());
        newUser.setSalt("xxx");
        newUser.setUserName(user.getValueOfUserName());
        auto c = mapper.update(newUser);
        MANDATE(c == 1);
        c = mapper.updateBy({Users::Cols::_avatar_id, Users::Cols::_salt},
                            Criteria(Users::Cols::_user_id,
                                     CompareOperator::EQ,
                                     "pg"),
                            "avatar of pg",
                            "salt of pg");
        MANDATE(c == 1);
        c = mapper.updateBy({Users::Cols::_avatar_id, Users::Cols::_salt},
                            Criteria(Users::Cols::_user_id,
                                     CompareOperator::EQ,
                                     "none"),
                            "avatar of none",
                            "salt of none");
        MANDATE(c == 0);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM mapper synchronous interface(1) what():",
              e.base().what());
    }

    /// Test ORM QueryBuilder
    /// execSync
    try
    {
        const std::vector<Users> users =
            QueryBuilder<Users>{}.from("users").selectAll().execSync(clientPtr);
        MANDATE(users.size() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM QueryBuilder synchronous interface(0) what():",
              e.base().what());
    }
    try
    {
        const Result users =
            QueryBuilder<Users>{}.from("users").select("id").execSync(
                clientPtr);
        MANDATE(users.size() == 2);
        for (const Row &u : users)
        {
            MANDATE(!u["id"].isNull());
        }
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM QueryBuilder synchronous interface(1) what():",
              e.base().what());
    }
    try
    {
        const Users user = QueryBuilder<Users>{}
                               .from("users")
                               .selectAll()
                               .eq("id", "2")
                               .limit(1)
                               .single()
                               .order("id", false)
                               .execSync(clientPtr);
        MANDATE(user.getPrimaryKey() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM QueryBuilder synchronous interface(2) what():",
              e.base().what());
    }
    try
    {
        const Row user = QueryBuilder<Users>{}
                             .from("users")
                             .select("id")
                             .limit(1)
                             .single()
                             .order("id", false)
                             .execSync(clientPtr);
        MANDATE(user["id"].as<int32_t>() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - ORM QueryBuilder synchronous interface(3) what():",
              e.base().what());
    }

    /// execAsyncFuture
    {
        std::future<std::vector<Users>> users =
            QueryBuilder<Users>{}.from("users").selectAll().execAsyncFuture(
                clientPtr);
        try
        {
            const std::vector<Users> r = users.get();
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "mysql - ORM QueryBuilder asynchronous interface(0) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Result> users =
            QueryBuilder<Users>{}.from("users").select("id").execAsyncFuture(
                clientPtr);
        try
        {
            const Result r = users.get();
            MANDATE(r.size() == 2);
            for (const Row &u : r)
            {
                MANDATE(!u["id"].isNull());
            }
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "mysql - ORM QueryBuilder asynchronous interface(1) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Users> user = QueryBuilder<Users>{}
                                      .from("users")
                                      .selectAll()
                                      .eq("id", "2")
                                      .limit(1)
                                      .single()
                                      .order("id", false)
                                      .execAsyncFuture(clientPtr);
        try
        {
            const Users r = user.get();
            MANDATE(r.getPrimaryKey() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "mysql - ORM QueryBuilder asynchronous interface(2) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Row> users = QueryBuilder<Users>{}
                                     .from("users")
                                     .select("id")
                                     .limit(1)
                                     .single()
                                     .order("id", false)
                                     .execAsyncFuture(clientPtr);
        try
        {
            const Row r = users.get();
            MANDATE(r["id"].as<int32_t>() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "mysql - ORM QueryBuilder asynchronous interface(3) "
                "what():",
                e.base().what());
        }
    }

#ifdef __cpp_impl_coroutine
    auto coro_test = [clientPtr, TEST_CTX]() -> drogon::Task<> {
        /// 7 Test coroutines.
        /// This is by no means comprehensive. But coroutine API is esentially a
        /// wrapper arround callbacks. The purpose is to test the interface
        /// works 7.1 Basic queries
        try
        {
            auto result =
                co_await clientPtr->execSqlCoro("select * from users;");
            MANDATE(result.size() != 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - DbClient coroutine interface(0) what():",
                  e.base().what());
        }
        /// 7.2 Parameter binding
        try
        {
            auto result = co_await clientPtr->execSqlCoro(
                "select * from users where 1=?;", 1);
            MANDATE(result.size() != 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - DbClient coroutine interface(1) what():",
                  e.base().what());
        }
    };
    drogon::sync_wait(coro_test());

#endif
}
#endif

#if USE_SQLITE3
DbClientPtr sqlite3Client;
DROGON_TEST(SQLite3Test)
{
    auto &clientPtr = sqlite3Client;
    REQUIRE(clientPtr != nullptr);

    // Prepare the test environment
    *clientPtr << "DROP TABLE IF EXISTS users" >> [TEST_CTX,
                                                   clientPtr](const Result &r) {
        SUCCESS();
        clientPtr->execSqlAsync(
            "select 1 as result",
            [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
            expFunction);
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("sqlite3 - Prepare the test environment(0):  what():",
              e.base().what());
    };

    *clientPtr << "CREATE TABLE users "
                  "("
                  "    id INTEGER PRIMARY KEY autoincrement,"
                  "    user_id varchar(32),"
                  "    user_name varchar(64),"
                  "    password varchar(64),"
                  "    org_name varchar(20),"
                  "    signature varchar(50),"
                  "    avatar_id varchar(32),"
                  "    salt character varchar(20),"
                  "    admin boolean DEFAULT false,"
                  "    create_time datetime,"
                  "    CONSTRAINT user_id_org UNIQUE(user_id, org_name)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(1) what():",
                  e.base().what());
        };
    /// Test1:DbClient streaming-type interface
    /// 1.1 insert,non-blocking
    *clientPtr << "insert into users "
                  "(user_id,user_name,password,org_name,create_time) "
                  "values(?,?,?,?,?)"
               << "pg"
               << "postgresql"
               << "123"
               << "default" << trantor::Date::now() >>
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() == 1ULL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(0) what():",
                  e.base().what());
        };
    /// 1.2 insert,blocking
    *clientPtr << "insert into users "
                  "(user_id,user_name,password,org_name,create_time) "
                  "values(?,?,?,?,?)"
               << "pg1"
               << "postgresql1"
               << "123"
               << "default" << trantor::Date::now() << Mode::Blocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() == 2ULL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(1) what():",
                  e.base().what());
        };
    *clientPtr << "select * from users where user_id=?;"
               << "pg1" << Mode::Blocking >>
        [TEST_CTX](const Result &r) {
            MANDATE(r.size() == 1);
            MANDATE(r[0]["user_id"].as<std::string>() == "pg1");
            using namespace drogon_model::sqlite3;
            Users user(r[0]);
            MANDATE(user.getValueOfUserId() == "pg1");
            // LOG_INFO << "user:" << user.toJson().toStyledString();
            MANDATE(trantor::Date::now().secondsSinceEpoch() -
                        user.getValueOfCreateTime().secondsSinceEpoch() <=
                    1);
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(2) what():",
                  e.base().what());
        };

    /// 1.3 query,no-blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::NonBlocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(2) what():",
                  e.base().what());
        };
    /// 1.4 query,blocking
    *clientPtr << "select * from users where 1 = 1" << Mode::Blocking >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(3) what():",
                  e.base().what());
        };
    /// 1.5 query,blocking
    int count = 0;
    *clientPtr << "select user_name, user_id, id from users where 1 = 1"
               << Mode::Blocking >>
        [&count, TEST_CTX](bool isNull,
                           const std::string &name,
                           std::string &&user_id,
                           int id) {
            if (!isNull)
                ++count;
            else
            {
                MANDATE(count == 2);
            }
        } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(4) what():",
                  e.base().what());
        };
    /// 1.6 query, parameter binding
    *clientPtr << "select * from users where id = ?" << 1 >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(5) what():",
                  e.base().what());
        };
    /// 1.7 query, parameter binding
    *clientPtr << "select * from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(6) what():",
                  e.base().what());
        };
    /// 1.8 delete
    *clientPtr << "delete from users where user_id = ? and user_name = ?"
               << "pg1"
               << "postgresql1" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(7) what():",
                  e.base().what());
        };
    /// 1.9 update
    *clientPtr << "update users set user_id = ?, user_name = ? where user_id "
                  "= ? and user_name = ?"
               << "pg1"
               << "postgresql1"
               << "pg"
               << "postgresql" >>
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1UL); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(8) what():",
                  e.base().what());
        };
    /// 1.10 clean up
    *clientPtr << "delete from users" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("sqlite3 - DbClient streaming-type interface(9.1) what():",
              e.base().what());
    };
    *clientPtr << "UPDATE sqlite_sequence SET seq = 0" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(9.2) what():",
                  e.base().what());
        };
    /// Test asynchronous method
    /// 2.1 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name,create_time) "
        "values(?,?,?,?,?)",
        [TEST_CTX](const Result &r) { MANDATE(r.insertId() == 1ULL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(0) what():",
                  e.base().what());
        },
        "pg",
        "postgresql",
        "123",
        "default",
        trantor::Date::now());
    /// 2.2 insert
    clientPtr->execSqlAsync(
        "insert into users "
        "(user_id,user_name,password,org_name,create_time) "
        "values(?,?,?,?,?)",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(1) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "123",
        "default",
        trantor::Date::now());
    /// 2.3 query
    clientPtr->execSqlAsync(
        "select * from users where 1 = 1",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 2UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 2.2 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where id = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(3) what():",
                  e.base().what());
        },
        1);
    /// 2.3 query, parameter binding
    clientPtr->execSqlAsync(
        "select * from users where user_id = ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(4) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1");
    /// 2.4 delete
    clientPtr->execSqlAsync(
        "delete from users where user_id = ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(5) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1");
    /// 2.5 update
    clientPtr->execSqlAsync(
        "update users set user_id = ?, user_name = ? where user_id "
        "= ? and user_name = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.affectedRows() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(6) what():",
                  e.base().what());
        },
        "pg1",
        "postgresql1",
        "pg",
        "postgresql");
    /// 2.6 clean up
    clientPtr->execSqlAsync(
        "delete from users",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(7.1) what():",
                  e.base().what());
        });
    clientPtr->execSqlAsync(
        "UPDATE sqlite_sequence SET seq = 0",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(7.2) what():",
                  e.base().what());
        });

    /// Test synchronous method
    /// 3.1 insert
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  "
            "(user_id,user_name,password,org_name,create_time) "
            "values(?,?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default",
            trantor::Date::now());
        MANDATE(r.insertId() == 1ULL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient asynchronous interface(0) what():",
              e.base().what());
    }
    /// 3.2 insert,(string_view)
    try
    {
        drogon::string_view sv("pg1");
        drogon::string_view sv1("postgresql1");
        drogon::string_view sv2("123");
        auto r = clientPtr->execSqlSync(
            "insert into users  "
            "(user_id,user_name,password,org_name,create_time) "
            "values(?,?,?,?,?)",
            sv,
            (const drogon::string_view &)sv1,
            std::move(sv2),
            drogon::string_view("default"),
            trantor::Date::now());
        MANDATE(r.affectedRows() == 1UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient asynchronous interface(1) what():",
              e.base().what());
    }
    /// 3.3 query
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg1",
            "postgresql1");
        MANDATE(r.size() == 1UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient asynchronous interface(2) what():",
              e.base().what());
    }
    /// 3.4 query for none
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name=?",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient asynchronous interface(3) what():",
              e.base().what());
    }
    /// 3.5 bad sql
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where user_id=? and user_name='1234'",
            "pg111",
            "postgresql1");
        MANDATE(r.size() == 0UL);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 3.6 clean up
    try
    {
        auto r = clientPtr->execSqlSync("delete from users");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    try
    {
        auto r = clientPtr->execSqlSync("UPDATE sqlite_sequence SET seq = 0");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    /// Test future interface
    /// 4.1 insert
    auto f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name,create_time) "
        "values(?,?,?,?,?) ",
        "pg",
        "postgresql",
        "123",
        "default",
        trantor::Date::now());
    try
    {
        auto r = f.get();
        MANDATE(r.insertId() == 1ULL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(0) what():",
              e.base().what());
    }
    /// 4.2 insert
    f = clientPtr->execSqlAsyncFuture(
        "insert into users  (user_id,user_name,password,org_name,create_time) "
        "values(?,?,?,?,?)",
        "pg1",
        "postgresql1",
        "123",
        "default",
        trantor::Date::now());
    try
    {
        auto r = f.get();
        MANDATE(r.affectedRows() == 1UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(1) what():",
              e.base().what());
    }
    /// 4.3 query
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg1",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(2) what():",
              e.base().what());
    }
    /// 4.4 query for none
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name=?",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0UL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(3) what():",
              e.base().what());
    }
    /// 4.5 bad sql
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where user_id=? and user_name='12'",
        "pg111",
        "postgresql1");
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 0UL);
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    /// 4.6 clean up
    f = clientPtr->execSqlAsyncFuture("delete from users");
    try
    {
        auto r = f.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(5.1) what():",
              e.base().what());
    }
    f = clientPtr->execSqlAsyncFuture("UPDATE sqlite_sequence SET seq = 0");
    try
    {
        auto r = f.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(5.2) what():",
              e.base().what());
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
        FAULT("sqlite3 - Result throwing exceptions(0)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }
    // 5.2 insert one just for setup
    try
    {
        auto r = clientPtr->execSqlSync(
            "insert into users  "
            "(user_id,user_name,password,org_name,create_time) "
            "values(?,?,?,?,?)",
            "pg",
            "postgresql",
            "123",
            "default",
            trantor::Date::now());
        MANDATE(r.insertId() == 1ULL);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - Row throwing exceptions(0) what():", e.base().what());
    }

    // 5.3 try to access nonexistent column by name
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row["imaginary_column"];
        FAULT("sqlite3 - Row throwing exceptions(1)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.4 try to access nonexistent column by index
    try
    {
        auto r = clientPtr->execSqlSync("select * from users");
        auto row = r.at(0);
        row.at(420);
        FAULT("sqlite3 - Row throwing exceptions(2)");
    }
    catch (const DrogonDbException &e)
    {
        SUCCESS();
    }

    // 5.5 cleanup
    try
    {
        auto r = clientPtr->execSqlSync("delete from users");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 -  Row throwing exceptions(3) what():", e.base().what());
    }
    try
    {
        auto r = clientPtr->execSqlSync("UPDATE sqlite_sequence SET seq = 0");
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 -  Row throwing exceptions(3) what():", e.base().what());
    }

    /// Test ORM mapper TODO
    /// 5.1 insert, noneblocking
    using namespace drogon_model::sqlite3;
    Mapper<Users> mapper(clientPtr);
    Users user;
    user.setUserId("pg");
    user.setUserName("postgres");
    user.setPassword("123");
    user.setOrgName("default");
    user.setCreateTime(trantor::Date::now());
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - ORM mapper asynchronous interface(0) what():",
                  e.base().what());
        });
    /// 5.2 insert
    user.setUserId("pg1");
    user.setUserName("postgres1");
    mapper.insert(
        user,
        [TEST_CTX](Users ret) { MANDATE(ret.getPrimaryKey() == 2UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - ORM mapper asynchronous interface(1) what():",
                  e.base().what());
        });
    /// 5.3 select where in
    mapper.findBy(
        Criteria(Users::Cols::_id,
                 CompareOperator::In,
                 std::vector<int32_t>{2, 200}),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - ORM mapper asynchronous interface(2) what():",
                  e.base().what());
        });
    /// 5.3.5 count
    mapper.count(
        Criteria(Users::Cols::_id, CompareOperator::EQ, 2),
        [TEST_CTX](const size_t c) { MANDATE(c == 1UL); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - ORM mapper asynchronous interface(3) what():",
                  e.base().what());
        });
    /// 5.3.6 custom where query
    mapper.findBy(
        Criteria("password is not null"_sql),
        [TEST_CTX](std::vector<Users> users) { MANDATE(users.size() == 2); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - ORM mapper asynchronous interface(4) what():",
                  e.base().what());
        });
    /// 5.4 find by primary key. blocking
    try
    {
        auto user = mapper.findByPrimaryKey(1);
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - ORM mapper synchronous interface(0) what():",
              e.base().what());
    }

    /// Test ORM QueryBuilder
    /// execSync
    try
    {
        const std::vector<Users> users =
            QueryBuilder<Users>{}.from("users").selectAll().execSync(clientPtr);
        MANDATE(users.size() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - ORM QueryBuilder synchronous interface(0) what():",
              e.base().what());
    }
    try
    {
        const Result users =
            QueryBuilder<Users>{}.from("users").select("id").execSync(
                clientPtr);
        MANDATE(users.size() == 2);
        for (const Row &u : users)
        {
            MANDATE(!u["id"].isNull());
        }
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - ORM QueryBuilder synchronous interface(1) what():",
              e.base().what());
    }
    try
    {
        const Users user = QueryBuilder<Users>{}
                               .from("users")
                               .selectAll()
                               .eq("id", "2")
                               .limit(1)
                               .single()
                               .order("id", false)
                               .execSync(clientPtr);
        MANDATE(user.getPrimaryKey() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - ORM QueryBuilder synchronous interface(2) what():",
              e.base().what());
    }
    try
    {
        const Row user = QueryBuilder<Users>{}
                             .from("users")
                             .select("id")
                             .limit(1)
                             .single()
                             .order("id", false)
                             .execSync(clientPtr);
        MANDATE(user["id"].as<int32_t>() == 2);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - ORM QueryBuilder synchronous interface(3) what():",
              e.base().what());
    }

    /// execAsyncFuture
    {
        std::future<std::vector<Users>> users =
            QueryBuilder<Users>{}.from("users").selectAll().execAsyncFuture(
                clientPtr);
        try
        {
            const std::vector<Users> r = users.get();
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "sqlite3 - ORM QueryBuilder asynchronous interface(0) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Result> users =
            QueryBuilder<Users>{}.from("users").select("id").execAsyncFuture(
                clientPtr);
        try
        {
            const Result r = users.get();
            MANDATE(r.size() == 2);
            for (const Row &u : r)
            {
                MANDATE(!u["id"].isNull());
            }
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "sqlite3 - ORM QueryBuilder asynchronous interface(1) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Users> user = QueryBuilder<Users>{}
                                      .from("users")
                                      .selectAll()
                                      .eq("id", "2")
                                      .limit(1)
                                      .single()
                                      .order("id", false)
                                      .execAsyncFuture(clientPtr);
        try
        {
            const Users r = user.get();
            MANDATE(r.getPrimaryKey() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "sqlite3 - ORM QueryBuilder asynchronous interface(2) "
                "what():",
                e.base().what());
        }
    }
    {
        std::future<Row> users = QueryBuilder<Users>{}
                                     .from("users")
                                     .select("id")
                                     .limit(1)
                                     .single()
                                     .order("id", false)
                                     .execAsyncFuture(clientPtr);
        try
        {
            const Row r = users.get();
            MANDATE(r["id"].as<int32_t>() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "sqlite3 - ORM QueryBuilder asynchronous interface(3) "
                "what():",
                e.base().what());
        }
    }

#ifdef __cpp_impl_coroutine
    auto coro_test = [clientPtr, TEST_CTX]() -> drogon::Task<> {
        /// 7 Test coroutines.
        /// This is by no means comprehensive. But coroutine API is esentially a
        /// wrapper arround callbacks. The purpose is to test the interface
        /// works 7.1 Basic queries
        try
        {
            auto result =
                co_await clientPtr->execSqlCoro("select * from users;");
            MANDATE(result.size() != 0UL);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - DbClient coroutine interface(0) what():",
                  e.base().what());
        }
        /// 7.2 Parameter binding
        try
        {
            auto result = co_await clientPtr->execSqlCoro(
                "select * from users where 1=?;", 1);
            MANDATE(result.size() != 0UL);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - DbClient coroutine interface(1) what():",
                  e.base().what());
        }
        /// 7.3 ORM CoroMapper
        try
        {
            auto mapper = CoroMapper<Users>(clientPtr);
            auto user = co_await mapper.findOne(
                Criteria(Users::Cols::_id, CompareOperator::EQ, 1));
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - CoroMapper coroutine interface(0) what():",
                  e.base().what());
        }
        try
        {
            auto mapper = CoroMapper<Users>(clientPtr);
            auto users = co_await mapper.findBy(
                Criteria(Users::Cols::_id, CompareOperator::EQ, 1));
            MANDATE(users.size() == 1UL);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - CoroMapper coroutine interface(1) what():",
                  e.base().what());
        }
        try
        {
            auto mapper = CoroMapper<Users>(clientPtr);
            auto n = co_await mapper.deleteByPrimaryKey(1);
            MANDATE(n == 1);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - CoroMapper coroutine interface(2) what():",
                  e.base().what());
        }
        co_await drogon::sleepCoro(
            trantor::EventLoop::getEventLoopOfCurrentThread(), 1.0s);
    };
    drogon::sync_wait(coro_test());

#endif
}
#endif

using namespace drogon;
int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);

#if USE_MYSQL
    mysqlClient = DbClient::newMysqlClient(
        "host=localhost port=3306 user=root client_encoding=utf8mb4", 1);
#endif
#if USE_POSTGRESQL
    postgreClient = DbClient::newPgClient(
        "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=12345 "
        "client_encoding=utf8",
        1,
        true);
#endif
#if USE_SQLITE3
    sqlite3Client = DbClient::newSqlite3Client("filename=:memory:", 1);
#endif
    const int testStatus = test::run(argc, argv);
    return testStatus;
}
