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

static void initFastDbClients(IOThreadStorage<orm::DbClientPtr> &storage,
                              const std::vector<trantor::EventLoop *> &ioLoops,
                              const std::string &connInfo,
                              ClientType dbType,
                              size_t connNum,
                              bool autoBatch,
                              double timeout)
{
    storage.init([&](orm::DbClientPtr &c, size_t idx) {
        assert(idx == ioLoops[idx]->index());
        LOG_TRACE << "create fast database client for the thread " << idx;
        c = std::shared_ptr<orm::DbClient>(
            new drogon::orm::DbClientLockFree(connInfo,
                                              ioLoops[idx],
                                              dbType,
#if LIBPQ_SUPPORTS_BATCH_MODE  // Bad code
                                              connNum,
                                              autoBatch));
#else
                                              connNum));
#endif
        if (timeout > 0.0)
        {
            c->setTimeout(timeout);
        }
    });
}

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    assert(dbClientsMap_.empty());
    assert(dbFastClientsMap_.empty());
    for (auto &dbInfo : dbInfos_)
    {
        if (std::holds_alternative<PostgresConfig>(dbInfo.config_))
        {
            auto &cfg = std::get<PostgresConfig>(dbInfo.config_);
            if (cfg.isFast)
            {
                dbFastClientsMap_[cfg.name] =
                    IOThreadStorage<orm::DbClientPtr>();
                initFastDbClients(dbFastClientsMap_[cfg.name],
                                  ioLoops,
                                  dbInfo.connectionInfo_,
                                  ClientType::PostgreSQL,
                                  cfg.connectionNumber,
                                  cfg.autoBatch,
                                  cfg.timeout);
            }
            else
            {
                dbClientsMap_[cfg.name] =
                    drogon::orm::DbClient::newPgClient(dbInfo.connectionInfo_,
                                                       cfg.connectionNumber,
                                                       cfg.autoBatch);
                if (cfg.timeout > 0.0)
                {
                    dbClientsMap_[cfg.name]->setTimeout(cfg.timeout);
                }
            }
        }
        else if (std::holds_alternative<MysqlConfig>(dbInfo.config_))
        {
            auto &cfg = std::get<MysqlConfig>(dbInfo.config_);

            if (cfg.isFast)
            {
                dbFastClientsMap_[cfg.name] =
                    IOThreadStorage<orm::DbClientPtr>();
                initFastDbClients(dbFastClientsMap_[cfg.name],
                                  ioLoops,
                                  dbInfo.connectionInfo_,
                                  ClientType::Mysql,
                                  cfg.connectionNumber,
                                  false,
                                  cfg.timeout);
            }
            else
            {
                dbClientsMap_[cfg.name] = drogon::orm::DbClient::newMysqlClient(
                    dbInfo.connectionInfo_, cfg.connectionNumber);
                if (cfg.timeout > 0.0)
                {
                    dbClientsMap_[cfg.name]->setTimeout(cfg.timeout);
                }
            }
        }
        else if (std::holds_alternative<Sqlite3Config>(dbInfo.config_))
        {
            auto &cfg = std::get<Sqlite3Config>(dbInfo.config_);
            dbClientsMap_[cfg.name] =
                drogon::orm::DbClient::newSqlite3Client(dbInfo.connectionInfo_,
                                                        cfg.connectionNumber);
            if (cfg.timeout > 0.0)
            {
                dbClientsMap_[cfg.name]->setTimeout(cfg.timeout);
            }
        }
    }
}

static std::string buildConnStr(const std::string &host,
                                unsigned short port,
                                const std::string &databaseName,
                                const std::string &username,
                                const std::string &password,
                                const std::string &characterSet)
{
    auto connStr =
        utils::formattedString("host=%s port=%u dbname=%s user=%s",
                               escapeConnString(host).c_str(),
                               port,
                               escapeConnString(databaseName).c_str(),
                               escapeConnString(username).c_str());
    if (!password.empty())
    {
        connStr += " password=";
        connStr += escapeConnString(password);
    }
    if (!characterSet.empty())
    {
        connStr += " client_encoding=";
        connStr += escapeConnString(characterSet);
    }
    return connStr;
}

void DbClientManager::addDbClient(const DbConfig &config)
{
    if (std::holds_alternative<PostgresConfig>(config))
    {
#if USE_POSTGRESQL
        auto &cfg = std::get<PostgresConfig>(config);
        auto connStr = buildConnStr(cfg.host,
                                    cfg.port,
                                    cfg.databaseName,
                                    cfg.username,
                                    cfg.password,
                                    cfg.characterSet);
        // For valid connection options, see:
        // https://www.postgresql.org/docs/16/libpq-connect.html#LIBPQ-CONNECT-OPTIONS
        if (!cfg.connectOptions.empty())
        {
            std::string optionStr = " options='";
            for (auto const &[key, value] : cfg.connectOptions)
            {
                optionStr += " -c ";
                optionStr += escapeConnString(key);
                optionStr += "=";
                optionStr += escapeConnString(value);
            }
            optionStr += "'";
            connStr += optionStr;
        }
        dbInfos_.emplace_back(DbInfo{connStr, config});
#else
        std::cout << "The PostgreSQL is not supported in current drogon build, "
                     "please install the development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else if (std::holds_alternative<MysqlConfig>(config))
    {
#if USE_MYSQL
        auto cfg = std::get<MysqlConfig>(config);
        auto connStr = buildConnStr(cfg.host,
                                    cfg.port,
                                    cfg.databaseName,
                                    cfg.username,
                                    cfg.password,
                                    cfg.characterSet);
        dbInfos_.emplace_back(DbInfo{connStr, config});
#else
        std::cout << "The Mysql is not supported in current drogon build, "
                     "please install the development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else if (std::holds_alternative<Sqlite3Config>(config))
    {
#if USE_SQLITE3
        auto cfg = std::get<Sqlite3Config>(config);
        std::string connStr = "filename=" + cfg.filename;
        dbInfos_.emplace_back(DbInfo{connStr, config});
#else
        std::cout << "The Sqlite3 is not supported in current drogon build, "
                     "please install the development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else
    {
        assert(false && "Should not happen, unknown DbConfig alternatives");
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

DbClientManager::~DbClientManager()
{
    for (auto &pair : dbClientsMap_)
    {
        pair.second->closeAll();
    }
    for (auto &pair : dbFastClientsMap_)
    {
        pair.second.init([](DbClientPtr &clientPtr, size_t index) {
            // the main loop;
            std::promise<void> p;
            auto f = p.get_future();
            drogon::getIOThreadStorageLoop(index)->runInLoop(
                [&clientPtr, &p]() {
                    clientPtr->closeAll();
                    p.set_value();
                });
            f.get();
        });
    }
}
