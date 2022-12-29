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
#include <unordered_map>

#if USE_POSTGRESQL
#include "postgresql_impl/PgListener.h"
#endif

using namespace drogon;
using namespace drogon::orm;

// NOTE: listeners are stored in a centralized way.
// In this case we have minimal invasion on DbClient implementations,
// but the defect is that we have to use lock.
static std::mutex clientListenerMutex;
static std::unordered_map<DbClientPtr, DbListenerPtr> clientListener;

DbListener::DbListener(std::shared_ptr<DbClient> dbClient)
    : dbClient_(std::move(dbClient))
{
}

DbListener::~DbListener()
{
    std::lock_guard<std::mutex> lock(clientListenerMutex);
    if (clientListener.erase(dbClient_) == 0)
    {
        LOG_FATAL << "DbClient of DbListener was not found in map!";
    }
}

std::shared_ptr<DbListener> DbListener::newDbListener(DbClientPtr dbClient)
{
    std::lock_guard<std::mutex> lock(clientListenerMutex);
    if (clientListener.find(dbClient) != clientListener.end())
    {
        LOG_ERROR << "Given DbClient is already used by another dbListener";
        return nullptr;
    }

    DbListenerPtr dbListener;
    switch (dbClient->type())
    {
        case ClientType::PostgreSQL:
        {
#if USE_POSTGRESQL
            dbListener = std::make_shared<PgListener>(std::move(dbClient));
            break;
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
    if (dbListener)
    {
        clientListener.emplace(dbListener->dbClient_, dbListener);
    }
    return dbListener;
}

std::shared_ptr<DbListener> DbListener::getDbClientListener(
    const std::shared_ptr<DbClient>& dbClient)
{
    std::lock_guard<std::mutex> lock(clientListenerMutex);
    auto iter = clientListener.find(dbClient);
    if (iter == clientListener.end())
    {
        return nullptr;
    }
    return iter->second;
}
