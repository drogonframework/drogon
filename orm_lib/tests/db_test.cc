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
#include <string_view>
#include <drogon/orm/QueryBuilder.h>
#include <trantor/utils/Logger.h>

#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mysql/Users.h"
#include "mysql/Wallets.h"
#include "mysql/Blog.h"
#include "mysql/Category.h"
#include "mysql/BlogTag.h"
#include "mysql/Tag.h"

#include "postgresql/Users.h"
#include "postgresql/Wallets.h"
#include "postgresql/Blog.h"
#include "postgresql/Category.h"
#include "postgresql/BlogTag.h"
#include "postgresql/Tag.h"

#include "sqlite3/Users.h"
#include "sqlite3/Wallets.h"
#include "sqlite3/Blog.h"
#include "sqlite3/Category.h"
#include "sqlite3/BlogTag.h"
#include "sqlite3/Tag.h"

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
    // wallets table one-to-one with users table
    *clientPtr << "DROP TABLE IF EXISTS wallets" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(2) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE wallets ("
                  "    id serial PRIMARY KEY,"
                  "    user_id character varying(32) DEFAULT NULL,"
                  "    amount numeric(16,2) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(3) what():",
                  e.base().what());
        };
    // blog
    *clientPtr << "DROP TABLE IF EXISTS blog" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("postgresql - Prepare the test environment(4) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE blog ("
                  "    id serial PRIMARY KEY,"
                  "    title character varying(30) DEFAULT NULL,"
                  "    category_id int4 DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(5) what():",
                  e.base().what());
        };
    // category table one-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS category" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(6) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE category ("
                  "    id serial PRIMARY KEY,"
                  "    name character varying(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(7) what():",
                  e.base().what());
        };
    // tag table many-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS tag" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("postgresql - Prepare the test environment(8) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE tag ("
                  "    id serial PRIMARY KEY,"
                  "    name character varying(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(9) what():",
                  e.base().what());
        };
    // blog_tag table is an intermediate table
    *clientPtr << "DROP TABLE IF EXISTS blog_tag" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(10) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE blog_tag ("
                  "    blog_id int4 NOT NULL,"
                  "    tag_id int4 NOT NULL,"
                  "    CONSTRAINT blog_tag_pkey PRIMARY KEY (blog_id, tag_id)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - Prepare the test environment(11) what():",
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
    /// 1.10 query with raw parameter
    auto rawParamData = std::make_shared<int>(htonl(3));
    auto rawParam = RawParameter{rawParamData,
                                 reinterpret_cast<char *>(rawParamData.get()),
                                 sizeof(int),
                                 1};
    *clientPtr << "select * from users where length(user_id)=$1" << rawParam >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(9) what():",
                  e.base().what());
        };

    /// 1.11 clean up
    *clientPtr << "truncate table users restart identity" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient streaming-type interface(10) what():",
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
    /// 2.3 query, parameter binding (std::string_view)
    clientPtr->execSqlAsync(
        "select * from users where user_id = $1 and user_name = $2",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(4) what():",
                  e.base().what());
        },
        std::string_view("pg1"),
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
    /// 2.6 query with raw parameter
    clientPtr->execSqlAsync(
        "select * from users where length(user_id)=$1",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(7) what():",
                  e.base().what());
        },
        rawParam);
    /// 2.7 clean up
    clientPtr->execSqlAsync(
        "truncate table users restart identity",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("postgresql - DbClient asynchronous interface(8) what():",
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
    /// 3.6 query with raw parameter
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where length(user_id)=$1", rawParam);
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient asynchronous interface(4) what():",
              e.base().what());
    }
    /// 3.7 clean up
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
    /// 4.6 query with raw parameter
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where length(user_id)=$1", rawParam);
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("postgresql - DbClient future interface(4) what():",
              e.base().what());
    }
    /// 4.7 clean up
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
        /// This is by no means comprehensive. But coroutine API is essentially
        /// a wrapper around callbacks. The purpose is to test the interface
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

    /// 8 Test ORM related query
    /// 8.1 async
    /// 8.1.1 one-to-one
    Mapper<Wallets> walletsMapper(clientPtr);

    /// prepare
    {
        Wallets wallet;
        wallet.setUserId("pg");
        wallet.setAmount("2000.00");
        try
        {
            walletsMapper.insert(wallet);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(0) what():",
                  e.base().what());
        }
    }

    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets r) {
                MANDATE(r.getValueOfAmount() == "2000.00");
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper related async one-to-one(0) "
                    "what():",
                    e.base().what());
            });
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets w) {
                FAULT("postgresql - ORM mapper related async one-to-one(1)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.2 one-to-many
    Mapper<Category> categoryMapper(clientPtr);
    Mapper<Blog> blogMapper(clientPtr);

    /// prepare
    {
        Category category;
        category.setName("category1");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(1) what():",
                  e.base().what());
        }
        category.setName("category2");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(2) what():",
                  e.base().what());
        }
        Blog blog;
        blog.setTitle("title1");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(3) what():",
                  e.base().what());
        }
        blog.setTitle("title2");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(4) what():",
                  e.base().what());
        }
        blog.setTitle("title3");
        blog.setCategoryId(3);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(5) what():",
                  e.base().what());
        }
    }

    /// categories to blogs
    {
        Category category;
        category.setId(1);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 2); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper related async one-to-many(0) "
                    "what():",
                    e.base().what());
            });
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 0); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper related async one-to-many(1) "
                    "what():",
                    e.base().what());
            });
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                MANDATE(r.getValueOfName() == "category1");
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper related async one-to-many(2) "
                    "what():",
                    e.base().what());
            });
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                FAULT("postgresql - ORM mapper related async one-to-many(3)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.3 many-to-many
    Mapper<BlogTag> blogTagMapper(clientPtr);
    Mapper<Tag> tagMapper(clientPtr);

    /// prepare
    {
        BlogTag blogTag;
        blogTag.setBlogId(1);
        blogTag.setTagId(1);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(6) what():",
                  e.base().what());
        }
        blogTag.setBlogId(1);
        blogTag.setTagId(2);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(7) what():",
                  e.base().what());
        }
        Tag tag;
        tag.setName("tag1");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(8) what():",
                  e.base().what());
        }
        tag.setName("tag2");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related prepare(9) what():",
                  e.base().what());
        }
    }

    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        blog.getTags(
            clientPtr,
            [TEST_CTX](std::vector<std::pair<Tag, BlogTag>> r) {
                MANDATE(r.size() == 2);
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "postgresql - ORM mapper related async many-to-many(0) "
                    "what():",
                    e.base().what());
            });
    }

    /// 8.2 async
    /// 8.2.1 one-to-one
    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        try
        {
            auto r = user.getWallet(clientPtr);
            MANDATE(r.getValueOfAmount() == "2000.00");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related sync one-to-one(0) what():",
                  e.base().what());
        }
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        try
        {
            auto r = user.getWallet(clientPtr);
            FAULT("postgresql - ORM mapper related sync one-to-one(1)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.2 one-to-many
    /// categories to blogs
    {
        Category category;
        category.setId(1);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related sync one-to-many(0) what():",
                  e.base().what());
        }
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related sync one-to-many(1) what():",
                  e.base().what());
        }
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        try
        {
            auto r = blog.getCategory(clientPtr);
            MANDATE(r.getValueOfName() == "category1");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("postgresql - ORM mapper related sync one-to-many(2) what():",
                  e.base().what());
        }
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        try
        {
            auto r = blog.getCategory(clientPtr);
            FAULT("postgresql - ORM mapper related sync one-to-many(3)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.3 many-to-many
    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        try
        {
            auto r = blog.getTags(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT(
                "postgresql - ORM mapper related sync many-to-many(0) what():",
                e.base().what());
        }
    }
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
    // wallets table one-to-one with users table
    *clientPtr << "DROP TABLE IF EXISTS wallets" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(3) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `wallets` ("
                  "    `id` int(11) AUTO_INCREMENT PRIMARY KEY,"
                  "    `user_id` varchar(32) DEFAULT NULL,"
                  "    `amount` decimal(16,2) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(4) what():",
                  e.base().what());
        };
    // blog
    *clientPtr << "DROP TABLE IF EXISTS blog" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - Prepare the test environment(5) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE `blog` ("
                  "    `id` int(11) AUTO_INCREMENT PRIMARY KEY,"
                  "    `title` varchar(30) DEFAULT NULL,"
                  "    `category_id` int(11) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(6) what():",
                  e.base().what());
        };
    // category table one-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS category" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(7) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `category` ("
                  "    `id` int(11) AUTO_INCREMENT PRIMARY KEY,"
                  "    `name` varchar(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(8) what():",
                  e.base().what());
        };
    // tag table many-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS tag" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - Prepare the test environment(9) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE `tag` ("
                  "    `id` int(11) AUTO_INCREMENT PRIMARY KEY,"
                  "    `name` varchar(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(10) what():",
                  e.base().what());
        };
    // blog_tag table is an intermediate table
    *clientPtr << "DROP TABLE IF EXISTS blog_tag" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(11) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `blog_tag` ("
                  "    `blog_id` int(11) NOT NULL,"
                  "    `tag_id` int(11) NOT NULL,"
                  "    PRIMARY KEY (`blog_id`,`tag_id`)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - Prepare the test environment(12) what():",
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
    /// 1.10 query with raw parameter
    // MariaDB uses little-endian, so the opposite of network ordering :P
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    auto rawParamData = std::make_shared<int>(3);
#else
    auto rawParamData = std::make_shared<int>(0x03000000);  // byteswapped 3
#endif
    auto rawParam = RawParameter{rawParamData,
                                 reinterpret_cast<char *>(rawParamData.get()),
                                 sizeof(int),
                                 internal::MySqlLong};
    *clientPtr << "select * from users where length(user_id)=?" << rawParam >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient streaming-type interface(9) what():",
                  e.base().what());
        };
    /// 1.11 truncate
    *clientPtr << "truncate table users" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("mysql - DbClient streaming-type interface(10) what():",
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
    /// 2.6 query with raw parameter
    clientPtr->execSqlAsync(
        "select * from users where length(user_id)=?",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(7) what():",
                  e.base().what());
        },
        rawParam);
    /// 2.7 truncate
    clientPtr->execSqlAsync(
        "truncate table users",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("mysql - DbClient asynchronous interface(9) what():",
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
    /// 3.6 query with raw parameter
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where length(user_id)=?", rawParam);
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient asynchronous interface(4) what():",
              e.base().what());
    }
    /// 3.7 truncate
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
    /// 4.6. query with raw parameter
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where length(user_id)=?", rawParam);
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(5) what():", e.base().what());
    }
    /// 4.7 truncate
    f = clientPtr->execSqlAsyncFuture("truncate table users");
    try
    {
        auto r = f.get();
        SUCCESS();
    }
    catch (const DrogonDbException &e)
    {
        FAULT("mysql - DbClient future interface(6) what():", e.base().what());
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
        /// This is by no means comprehensive. But coroutine API is essentially
        /// a wrapper around callbacks. The purpose is to test the interface
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

    /// 8 Test ORM related query
    /// 8.1 async
    /// 8.1.1 one-to-one
    Mapper<Wallets> walletsMapper(clientPtr);

    /// prepare
    {
        Wallets wallet;
        wallet.setUserId("pg");
        wallet.setAmount("2000.00");
        try
        {
            walletsMapper.insert(wallet);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(0) what():",
                  e.base().what());
        }
    }

    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets r) {
                MANDATE(r.getValueOfAmount() == "2000.00");
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT("mysql - ORM mapper related async one-to-one(0) what():",
                      e.base().what());
            });
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets w) {
                FAULT("mysql - ORM mapper related async one-to-one(1)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.2 one-to-many
    Mapper<Category> categoryMapper(clientPtr);
    Mapper<Blog> blogMapper(clientPtr);

    /// prepare
    {
        Category category;
        category.setName("category1");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(1) what():",
                  e.base().what());
        }
        category.setName("category2");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(2) what():",
                  e.base().what());
        }
        Blog blog;
        blog.setTitle("title1");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(3) what():",
                  e.base().what());
        }
        blog.setTitle("title2");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(4) what():",
                  e.base().what());
        }
        blog.setTitle("title3");
        blog.setCategoryId(3);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(5) what():",
                  e.base().what());
        }
    }

    /// categories to blogs
    {
        Category category;
        category.setId(1);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 2); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT("mysql - ORM mapper related async one-to-many(0) what():",
                      e.base().what());
            });
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 0); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT("mysql - ORM mapper related async one-to-many(1) what():",
                      e.base().what());
            });
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                MANDATE(r.getValueOfName() == "category1");
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT("mysql - ORM mapper related async one-to-many(2) what():",
                      e.base().what());
            });
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                FAULT("mysql - ORM mapper related async one-to-many(3)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.3 many-to-many
    Mapper<BlogTag> blogTagMapper(clientPtr);
    Mapper<Tag> tagMapper(clientPtr);

    /// prepare
    {
        BlogTag blogTag;
        blogTag.setBlogId(1);
        blogTag.setTagId(1);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(6) what():",
                  e.base().what());
        }
        blogTag.setBlogId(1);
        blogTag.setTagId(2);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(7) what():",
                  e.base().what());
        }
        Tag tag;
        tag.setName("tag1");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(8) what():",
                  e.base().what());
        }
        tag.setName("tag2");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related prepare(9) what():",
                  e.base().what());
        }
    }

    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        blog.getTags(
            clientPtr,
            [TEST_CTX](std::vector<std::pair<Tag, BlogTag>> r) {
                MANDATE(r.size() == 2);
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "mysql - ORM mapper related async many-to-many(0) what():",
                    e.base().what());
            });
    }

    /// 8.2 async
    /// 8.2.1 one-to-one
    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        try
        {
            auto r = user.getWallet(clientPtr);
            MANDATE(r.getValueOfAmount() == "2000.00");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related sync one-to-one(0) what():",
                  e.base().what());
        }
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        try
        {
            auto r = user.getWallet(clientPtr);
            FAULT("mysql - ORM mapper related sync one-to-one(1)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.2 one-to-many
    /// categories to blogs
    {
        Category category;
        category.setId(1);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related sync one-to-many(0) what():",
                  e.base().what());
        }
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related sync one-to-many(1) what():",
                  e.base().what());
        }
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        try
        {
            auto r = blog.getCategory(clientPtr);
            MANDATE(r.getValueOfName() == "category1");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related sync one-to-many(2) what():",
                  e.base().what());
        }
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        try
        {
            auto r = blog.getCategory(clientPtr);
            FAULT("mysql - ORM mapper related sync one-to-many(3)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.3 many-to-many
    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        try
        {
            auto r = blog.getTags(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("mysql - ORM mapper related sync many-to-many(0) what():",
                  e.base().what());
        }
    }
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
    // wallets table one-to-one with users table
    *clientPtr << "DROP TABLE IF EXISTS wallets" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(2) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `wallets` ("
                  "    `id` INTEGER PRIMARY KEY autoincrement,"
                  "    `user_id` varchar(32) DEFAULT NULL,"
                  "    `amount` decimal(16,2) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(3) what():",
                  e.base().what());
        };
    // blog
    *clientPtr << "DROP TABLE IF EXISTS blog" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("sqlite3 - Prepare the test environment(4) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE `blog` ("
                  "    `id` INTEGER PRIMARY KEY autoincrement,"
                  "    `title` varchar(30) DEFAULT NULL,"
                  "    `category_id` INTEGER DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(5) what():",
                  e.base().what());
        };
    // category table one-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS category" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(6) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `category` ("
                  "    `id` INTEGER PRIMARY KEY autoincrement,"
                  "    `name` varchar(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(7) what():",
                  e.base().what());
        };
    // tag table many-to-many with blog table
    *clientPtr << "DROP TABLE IF EXISTS tag" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("sqlite3 - Prepare the test environment(8) what():",
              e.base().what());
    };
    *clientPtr << "CREATE TABLE `tag` ("
                  "    `id` INTEGER PRIMARY KEY autoincrement,"
                  "    `name` varchar(30) DEFAULT NULL"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(9) what():",
                  e.base().what());
        };
    // blog_tag table is an intermediate table
    *clientPtr << "DROP TABLE IF EXISTS blog_tag" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(10) what():",
                  e.base().what());
        };
    *clientPtr << "CREATE TABLE `blog_tag` ("
                  "    `blog_id` INTEGER NOT NULL,"
                  "    `tag_id` INTEGER NOT NULL,"
                  "    PRIMARY KEY (`blog_id`,`tag_id`)"
                  ")" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - Prepare the test environment(11) what():",
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
    /// 1.10 query with raw parameter
    auto rawParamData = std::make_shared<int>(3);
    auto rawParam = RawParameter{rawParamData,
                                 reinterpret_cast<char *>(rawParamData.get()),
                                 0,
                                 Sqlite3TypeInt};
    *clientPtr << "select * from users where length(user_id) = ?" << rawParam >>
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(9) what():",
                  e.base().what());
        };
    /// 1.11 clean up
    *clientPtr << "delete from users" >> [TEST_CTX](const Result &r) {
        SUCCESS();
    } >> [TEST_CTX](const DrogonDbException &e) {
        FAULT("sqlite3 - DbClient streaming-type interface(10.1) what():",
              e.base().what());
    };
    *clientPtr << "UPDATE sqlite_sequence SET seq = 0" >>
        [TEST_CTX](const Result &r) { SUCCESS(); } >>
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient streaming-type interface(10.2) what():",
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
    /// 2.6 query with raw parameter
    clientPtr->execSqlAsync(
        "select * from users where length(user_id) = ?",
        [TEST_CTX](const Result &r) { MANDATE(r.size() == 1); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(7) what():",
                  e.base().what());
        },
        rawParam);
    /// 2.7 clean up
    clientPtr->execSqlAsync(
        "delete from users",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(8.1) what():",
                  e.base().what());
        });
    clientPtr->execSqlAsync(
        "UPDATE sqlite_sequence SET seq = 0",
        [TEST_CTX](const Result &r) { SUCCESS(); },
        [TEST_CTX](const DrogonDbException &e) {
            FAULT("sqlite3 - DbClient asynchronous interface(8.2) what():",
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
    /// 3.2 insert,(std::string_view)
    try
    {
        std::string_view sv("pg1");
        std::string_view sv1("postgresql1");
        std::string_view sv2("123");
        auto r = clientPtr->execSqlSync(
            "insert into users  "
            "(user_id,user_name,password,org_name,create_time) "
            "values(?,?,?,?,?)",
            sv,
            (const std::string_view &)sv1,
            std::move(sv2),
            std::string_view("default"),
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
    /// 3.6 query with raw parameter
    try
    {
        auto r = clientPtr->execSqlSync(
            "select * from users where length(user_id) = ?", rawParam);
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient asynchronous interface(4) what():",
              e.base().what());
    }
    /// 3.7 clean up
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
    /// 4.6 query with raw parameter
    f = clientPtr->execSqlAsyncFuture(
        "select * from users where length(user_id)=?", rawParam);
    try
    {
        auto r = f.get();
        MANDATE(r.size() == 1);
    }
    catch (const DrogonDbException &e)
    {
        FAULT("sqlite3 - DbClient future interface(4) what():",
              e.base().what());
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
        /// This is by no means comprehensive. But coroutine API is essentially
        /// a wrapper around callbacks. The purpose is to test the interface
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

    /// 8 Test ORM related query
    /// 8.1 async
    /// 8.1.1 one-to-one
    Mapper<Wallets> walletsMapper(clientPtr);

    /// prepare
    {
        Wallets wallet;
        wallet.setUserId("pg");
        wallet.setAmount("2000.00");
        try
        {
            walletsMapper.insert(wallet);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(0) what():",
                  e.base().what());
        }
    }

    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets r) { MANDATE(r.getValueOfAmount() == "2000"); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "sqlite3 - ORM mapper related async one-to-one(0) what(): ",
                    e.base().what());
            });
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        user.getWallet(
            clientPtr,
            [TEST_CTX](Wallets w) {
                FAULT("sqlite3 - ORM mapper related async one-to-one(1)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.2 one-to-many
    Mapper<Category> categoryMapper(clientPtr);
    Mapper<Blog> blogMapper(clientPtr);

    /// prepare
    {
        Category category;
        category.setName("category1");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(1) what():",
                  e.base().what());
        }
        category.setName("category2");
        try
        {
            categoryMapper.insert(category);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(2) what():",
                  e.base().what());
        }
        Blog blog;
        blog.setTitle("title1");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(3) what():",
                  e.base().what());
        }
        blog.setTitle("title2");
        blog.setCategoryId(1);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(4) what():",
                  e.base().what());
        }
        blog.setTitle("title3");
        blog.setCategoryId(3);
        try
        {
            blogMapper.insert(blog);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(5) what():",
                  e.base().what());
        }
    }

    /// categories to blogs
    {
        Category category;
        category.setId(1);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 2); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "sqlite3 - ORM mapper related async one-to-many(0) "
                    "what(): ",
                    e.base().what());
            });
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        category.getBlogs(
            clientPtr,
            [TEST_CTX](std::vector<Blog> r) { MANDATE(r.size() == 0); },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "sqlite3 - ORM mapper related async one-to-many(1) "
                    "what(): ",
                    e.base().what());
            });
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                MANDATE(r.getValueOfName() == "category1");
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "sqlite3 - ORM mapper related async one-to-many(2) "
                    "what(): ",
                    e.base().what());
            });
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        blog.getCategory(
            clientPtr,
            [TEST_CTX](Category r) {
                FAULT("sqlite3 - ORM mapper related async one-to-many(3)");
            },
            [TEST_CTX](const DrogonDbException &e) { SUCCESS(); });
    }

    /// 8.1.3 many-to-many
    Mapper<BlogTag> blogTagMapper(clientPtr);
    Mapper<Tag> tagMapper(clientPtr);

    /// prepare
    {
        BlogTag blogTag;
        blogTag.setBlogId(1);
        blogTag.setTagId(1);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(6) what():",
                  e.base().what());
        }
        blogTag.setBlogId(1);
        blogTag.setTagId(2);
        try
        {
            blogTagMapper.insert(blogTag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(7) what():",
                  e.base().what());
        }
        Tag tag;
        tag.setName("tag1");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(8) what():",
                  e.base().what());
        }
        tag.setName("tag2");
        try
        {
            tagMapper.insert(tag);
            SUCCESS();
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related prepare(9) what():",
                  e.base().what());
        }
    }

    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        blog.getTags(
            clientPtr,
            [TEST_CTX](std::vector<std::pair<Tag, BlogTag>> r) {
                MANDATE(r.size() == 2);
            },
            [TEST_CTX](const DrogonDbException &e) {
                FAULT(
                    "sqlite3 - ORM mapper related async many-to-many(0) "
                    "what():",
                    e.base().what());
            });
    }

    /// 8.2 async
    /// 8.2.1 one-to-one
    /// users to wallets
    {
        Users user;
        user.setUserId("pg");
        try
        {
            auto r = user.getWallet(clientPtr);
            MANDATE(r.getValueOfAmount() == "2000");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related sync one-to-one(0) what():",
                  e.base().what());
        }
    }
    /// users to wallets without data
    {
        Users user;
        user.setUserId("pg1");
        try
        {
            auto r = user.getWallet(clientPtr);
            FAULT("sqlite3 - ORM mapper related sync one-to-one(1)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.2 one-to-many
    /// categories to blogs
    {
        Category category;
        category.setId(1);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related sync one-to-many(0) what():",
                  e.base().what());
        }
    }
    /// categories to blogs without data
    {
        Category category;
        category.setId(2);
        try
        {
            auto r = category.getBlogs(clientPtr);
            MANDATE(r.size() == 0);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related sync one-to-many(1) what():",
                  e.base().what());
        }
    }
    /// blogs to categories
    {
        Blog blog;
        blog.setCategoryId(1);
        try
        {
            auto r = blog.getCategory(clientPtr);
            MANDATE(r.getValueOfName() == "category1");
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related sync one-to-many(2) what():",
                  e.base().what());
        }
    }
    /// blogs to categories without data
    {
        Blog blog;
        blog.setCategoryId(3);
        try
        {
            auto r = blog.getCategory(clientPtr);
            FAULT("sqlite3 - ORM mapper related sync one-to-many(3)");
        }
        catch (const DrogonDbException &e)
        {
            SUCCESS();
        }
    }

    /// 8.2.3 many-to-many
    /// blogs to tags
    {
        Blog blog;
        blog.setId(1);
        try
        {
            auto r = blog.getTags(clientPtr);
            MANDATE(r.size() == 2);
        }
        catch (const DrogonDbException &e)
        {
            FAULT("sqlite3 - ORM mapper related sync many-to-many(0) what():",
                  e.base().what());
        }
    }
}
#endif

using namespace drogon;

int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);

#if USE_MYSQL
    mysqlClient = DbClient::newMysqlClient(
        "host=127.0.0.1 port=3306 user=root client_encoding=utf8mb4", 1);
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
