/**
 *
 *  DbClientManager.cc
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

#include "DbClientManager.h"
#include "DbClientLockFree.h"
#include <drogon/utils/Utilities.h>
#include <algorithm>

using namespace drogon::orm;
using namespace drogon;

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> &ioloops)
{
    assert(_dbClientsMap.empty());
    assert(_dbFastClientsMap.empty());
    for (auto &dbInfo : _dbInfos)
    {
        if (dbInfo._isFast)
        {
            for (auto *loop : ioloops)
            {
                if (dbInfo._dbType == drogon::orm::ClientType::Sqlite3)
                {
                    LOG_ERROR << "Sqlite3 don't support fast mode";
                    abort();
                }
                if (dbInfo._dbType == drogon::orm::ClientType::PostgreSQL ||
                    dbInfo._dbType == drogon::orm::ClientType::Mysql)
                {
                    _dbFastClientsMap[dbInfo._name][loop] =
                        std::shared_ptr<drogon::orm::DbClient>(
                            new drogon::orm::DbClientLockFree(
                                dbInfo._connectionInfo,
                                loop,
                                dbInfo._dbType,
                                dbInfo._connectionNumber));
                }
            }
        }
        else
        {
            if (dbInfo._dbType == drogon::orm::ClientType::PostgreSQL)
            {
#if USE_POSTGRESQL
                _dbClientsMap[dbInfo._name] =
                    drogon::orm::DbClient::newPgClient(
                        dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
            else if (dbInfo._dbType == drogon::orm::ClientType::Mysql)
            {
#if USE_MYSQL
                _dbClientsMap[dbInfo._name] =
                    drogon::orm::DbClient::newMysqlClient(
                        dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
            else if (dbInfo._dbType == drogon::orm::ClientType::Sqlite3)
            {
#if USE_SQLITE3
                _dbClientsMap[dbInfo._name] =
                    drogon::orm::DbClient::newSqlite3Client(
                        dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
        }
    }
}

void DbClientManager::createDbClient(const std::string &dbType,
                                     const std::string &host,
                                     const u_short port,
                                     const std::string &databaseName,
                                     const std::string &userName,
                                     const std::string &password,
                                     const size_t connectionNum,
                                     const std::string &filename,
                                     const std::string &name,
                                     const bool isFast)
{
    auto connStr = utils::formattedString("host=%s port=%u dbname=%s user=%s",
                                          host.c_str(),
                                          port,
                                          databaseName.c_str(),
                                          userName.c_str());
    if (!password.empty())
    {
        connStr += " password=";
        connStr += password;
    }
    std::string type = dbType;
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    DbInfo info;
    info._connectionInfo = connStr;
    info._connectionNumber = connectionNum;
    info._isFast = isFast;
    info._name = name;

    if (type == "postgresql")
    {
#if USE_POSTGRESQL
        info._dbType = orm::ClientType::PostgreSQL;
        _dbInfos.push_back(info);
#else
        std::cout
            << "The PostgreSQL is not supported by drogon, please install "
               "the development library first."
            << std::endl;
        exit(1);
#endif
    }
    else if (type == "mysql")
    {
#if USE_MYSQL
        info._dbType = orm::ClientType::Mysql;
        _dbInfos.push_back(info);
#else
        std::cout << "The Mysql is not supported by drogon, please install the "
                     "development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else if (type == "sqlite3")
    {
#if USE_SQLITE3
        std::string sqlite3ConnStr = "filename=" + filename;
        info._connectionInfo = sqlite3ConnStr;
        info._dbType = orm::ClientType::Sqlite3;
        _dbInfos.push_back(info);
#else
        std::cout
            << "The Sqlite3 is not supported by drogon, please install the "
               "development library first."
            << std::endl;
        exit(1);
#endif
    }
}