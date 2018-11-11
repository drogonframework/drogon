/**
 *
 *  DbClient.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include <drogon/orm/DbClient.h>
#if USE_POSTGRESQL
#include "postgresql_impl/PgClientImpl.h"
#endif
using namespace drogon::orm;
using namespace drogon;

internal::SqlBinder DbClient::operator << (const std::string &sql)
{
    return internal::SqlBinder(sql,*this);
}

#if USE_POSTGRESQL
std::shared_ptr<DbClient> DbClient::newPgClient(const std::string &connInfo, const size_t connNum)
{
    return std::make_shared<PgClientImpl>(connInfo, connNum);
}
#endif