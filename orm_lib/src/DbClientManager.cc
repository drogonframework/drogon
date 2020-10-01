/**
 *
 *  @file DbClientManager.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "../../lib/src/DbClientManager.h"
#include "DbClientLockFree.h"
#include <drogon/config.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>

using namespace drogon::orm;
using namespace drogon;

inline std::string escapeConnString(const std::string &str)
{
    bool beQuoted = str.empty() || (str.find(' ') != std::string::npos);

    std::string escaped;
    escaped.reserve(str.size());
    for (auto ch : str)
    {
        if (ch == '\'')
            escaped.push_back('\\');
        else if (ch == '\\')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }

    if (beQuoted)
        return "'" + escaped + "'";
    return escaped;
}

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> &ioloops)
{
    assert(dbClientsMap_.empty());
    assert(dbFastClientsMap_.empty());
    for (auto &dbInfo : dbInfos_)
    {
        if (dbInfo.isFast_)
        {
            if (dbInfo.dbType_ == drogon::orm::ClientType::Sqlite3)
            {
                LOG_ERROR << "Sqlite3 don't support fast mode";
                abort();
            }
            if (dbInfo.dbType_ == drogon::orm::ClientType::PostgreSQL ||
                dbInfo.dbType_ == drogon::orm::ClientType::Mysql)
            {
                dbFastClientsMap_[dbInfo.name_] =
                    IOThreadStorage<orm::DbClientPtr>();
                dbFastClientsMap_[dbInfo.name_].init([&](orm::DbClientPtr &c,
                                                         size_t idx) {
                    assert(idx == ioloops[idx]->index());
                    LOG_TRACE << "create fast database client for the thread "
                              << idx;
                    c = std::shared_ptr<orm::DbClient>(
                        new drogon::orm::DbClientLockFree(
                            dbInfo.connectionInfo_,
                            ioloops[idx],
                            dbInfo.dbType_,
                            dbInfo.connectionNumber_));
                });
            }
        }
        else
        {
            if (dbInfo.dbType_ == drogon::orm::ClientType::PostgreSQL)
            {
#if USE_POSTGRESQL
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newPgClient(
                        dbInfo.connectionInfo_, dbInfo.connectionNumber_);
#endif
            }
            else if (dbInfo.dbType_ == drogon::orm::ClientType::Mysql)
            {
#if USE_MYSQL
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newMysqlClient(
                        dbInfo.connectionInfo_, dbInfo.connectionNumber_);
#endif
            }
            else if (dbInfo.dbType_ == drogon::orm::ClientType::Sqlite3)
            {
#if USE_SQLITE3
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newSqlite3Client(
                        dbInfo.connectionInfo_, dbInfo.connectionNumber_);
#endif
            }
        }
    }
}

void DbClientManager::createDbClient(const std::string &dbType,
                                     const std::string &host,
                                     const unsigned short port,
                                     const std::string &databaseName,
                                     const std::string &userName,
                                     const std::string &password,
                                     const size_t connectionNum,
                                     const std::string &filename,
                                     const std::string &name,
                                     const bool isFast,
                                     const std::string &characterSet)
{
    auto connStr =
        utils::formattedString("host=%s port=%u dbname=%s user=%s",
                               escapeConnString(host).c_str(),
                               port,
                               escapeConnString(databaseName).c_str(),
                               escapeConnString(userName).c_str());
    if (!password.empty())
    {
        connStr += " password=";
        connStr += escapeConnString(password);
    }
    std::string type = dbType;
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (!characterSet.empty())
    {
        connStr += " client_encoding=";
        connStr += escapeConnString(characterSet);
    }
    DbInfo info;
    info.connectionInfo_ = connStr;
    info.connectionNumber_ = connectionNum;
    info.isFast_ = isFast;
    info.name_ = name;

    if (type == "postgresql")
    {
#if USE_POSTGRESQL
        info.dbType_ = orm::ClientType::PostgreSQL;
        dbInfos_.push_back(info);
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
        info.dbType_ = orm::ClientType::Mysql;
        dbInfos_.push_back(info);
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
        info.connectionInfo_ = sqlite3ConnStr;
        info.dbType_ = orm::ClientType::Sqlite3;
        dbInfos_.push_back(info);
#else
        std::cout
            << "The Sqlite3 is not supported by drogon, please install the "
               "development library first."
            << std::endl;
        exit(1);
#endif
    }
}

bool DbClientManager::areAllDbClientsAvailable() const noexcept
{
    for (auto const &pair : dbClientsMap_)
    {
        if (!(pair.second)->hasAvailableConnections())
            return false;
    }
    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
    if (loop && loop->index() < app().getThreadNum())
    {
        for (auto const &pair : dbFastClientsMap_)
        {
            if (!(*(pair.second))->hasAvailableConnections())
                return false;
        }
    }
    return true;
}
