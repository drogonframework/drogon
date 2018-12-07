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

#include <drogon/orm/DbClient.h>
#include "DbClientImpl.h"
using namespace drogon::orm;
using namespace drogon;

internal::SqlBinder DbClient::operator<<(const std::string &sql)
{
    return internal::SqlBinder(sql, *this, _type);
}

#if USE_POSTGRESQL
std::shared_ptr<DbClient> DbClient::newPgClient(const std::string &connInfo, const size_t connNum)
{
    return std::make_shared<DbClientImpl>(connInfo, connNum, ClientType::PostgreSQL);
}
#endif

#if USE_MYSQL
std::shared_ptr<DbClient> DbClient::newMysqlClient(const std::string &connInfo, const size_t connNum)
{
    return std::make_shared<DbClientImpl>(connInfo, connNum, ClientType::Mysql);
}
#endif
