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

orm::internal::SqlBinder DbClient::operator<<(const std::string &sql)
{
    return orm::internal::SqlBinder(sql, *this, type_);
}

orm::internal::SqlBinder DbClient::operator<<(std::string &&sql)
{
    return orm::internal::SqlBinder(std::move(sql), *this, type_);
}

std::shared_ptr<DbClient> DbClient::newPgClient(const std::string &connInfo,
                                                const size_t connNum)
{
#if USE_POSTGRESQL
    return std::make_shared<DbClientImpl>(connInfo,
                                          connNum,
                                          ClientType::PostgreSQL);
#else
    LOG_FATAL << "PostgreSQL is not supported!";
    exit(1);
#endif
}

std::shared_ptr<DbClient> DbClient::newMysqlClient(const std::string &connInfo,
                                                   const size_t connNum)
{
#if USE_MYSQL
    return std::make_shared<DbClientImpl>(connInfo, connNum, ClientType::Mysql);
#else
    LOG_FATAL << "Mysql is not supported!";
    exit(1);
#endif
}

std::shared_ptr<DbClient> DbClient::newSqlite3Client(
    const std::string &connInfo,
    const size_t connNum)
{
#if USE_SQLITE3
    return std::make_shared<DbClientImpl>(connInfo,
                                          connNum,
                                          ClientType::Sqlite3);
#else
    LOG_FATAL << "Sqlite3 is not supported!";
    exit(1);
#endif
}
