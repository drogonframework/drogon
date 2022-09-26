/**
 *
 *  DbClient.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbClientImpl.h"
#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
using namespace drogon::orm;
using namespace drogon;

DbClient::~DbClient() = default;

orm::internal::SqlBinder DbClient::operator<<(const std::string &sql)
{
    return orm::internal::SqlBinder(sql, *this, type_);
}

orm::internal::SqlBinder DbClient::operator<<(std::string &&sql)
{
    return orm::internal::SqlBinder(std::move(sql), *this, type_);
}

std::shared_ptr<DbClient> DbClient::newPgClient(const std::string &connInfo,
                                                size_t connNum,
                                                bool autoBatch)
{
#if USE_POSTGRESQL
    auto client = std::make_shared<DbClientImpl>(connInfo,
                                                 connNum,
#if LIBPQ_SUPPORTS_BATCH_MODE
                                                 ClientType::PostgreSQL,
                                                 autoBatch);
#else
                                                 ClientType::PostgreSQL);
#endif
    client->init();
    return client;
#else
    LOG_FATAL << "PostgreSQL is not supported!";
    exit(1);
    (void)(connInfo);
    (void)(connNum);
#endif
}

std::shared_ptr<DbClient> DbClient::newMysqlClient(const std::string &connInfo,
                                                   size_t connNum)
{
#if USE_MYSQL
    auto client = std::make_shared<DbClientImpl>(connInfo,
                                                 connNum,
#if LIBPQ_SUPPORTS_BATCH_MODE
                                                 ClientType::Mysql,
                                                 false);
#else
                                                 ClientType::Mysql);
#endif
    client->init();
    return client;
#else
    LOG_FATAL << "Mysql is not supported!";
    exit(1);
    (void)(connInfo);
    (void)(connNum);
#endif
}

std::shared_ptr<DbClient> DbClient::newSqlite3Client(
    const std::string &connInfo,
    size_t connNum)
{
#if USE_SQLITE3
    auto client = std::make_shared<DbClientImpl>(connInfo,
                                                 connNum,
#if LIBPQ_SUPPORTS_BATCH_MODE
                                                 ClientType::Sqlite3,
                                                 false);
#else
                                                 ClientType::Sqlite3);
#endif
    client->init();
    return client;
#else
    LOG_FATAL << "Sqlite3 is not supported!";
    exit(1);
    (void)(connInfo);
    (void)(connNum);
#endif
}
