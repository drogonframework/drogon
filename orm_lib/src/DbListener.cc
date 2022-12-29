/**
 *
 *  @file DbListener.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, An Tao.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/config.h>
#include <drogon/orm/DbListener.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <mutex>

#if USE_POSTGRESQL
#include "postgresql_impl/PgListener.h"
#endif

using namespace drogon;
using namespace drogon::orm;

DbListener::~DbListener() = default;

std::shared_ptr<DbListener> DbListener::newDbListener(DbClientPtr dbClient)
{
    switch (dbClient->type())
    {
        case ClientType::PostgreSQL:
        {
#if USE_POSTGRESQL
            return std::make_shared<PgListener>(std::move(dbClient));
#else
            LOG_ERROR << "Postgresql is not supported by current drogon build";
            return nullptr;
#endif
        }
        case ClientType::Mysql:
        {
            LOG_ERROR << "DbListener does not support mysql.";
            return nullptr;
        }
        case ClientType::Sqlite3:
        {
            LOG_ERROR << "DbListener does not support sqlite3.";
            return nullptr;
        }
    }
}
