/**
 *
 *  DbClientManager.h
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

#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/IOThreadStorage.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <string>
#include <memory>

namespace drogon
{
namespace orm
{
class DbClientManager : public trantor::NonCopyable
{
  public:
    void createDbClients(const std::vector<trantor::EventLoop *> &ioloops);
    DbClientPtr getDbClient(const std::string &name)
    {
        assert(_dbClientsMap.find(name) != _dbClientsMap.end());
        return _dbClientsMap[name];
    }

    DbClientPtr getFastDbClient(const std::string &name)
    {
        auto iter = _dbFastClientsMap.find(name);
        assert(iter != _dbFastClientsMap.end());
        return iter->second.getThreadData();
    }
    void createDbClient(const std::string &dbType,
                        const std::string &host,
                        const unsigned short port,
                        const std::string &databaseName,
                        const std::string &userName,
                        const std::string &password,
                        const size_t connectionNum,
                        const std::string &filename,
                        const std::string &name,
                        const bool isFast);

  private:
    std::map<std::string, DbClientPtr> _dbClientsMap;
    struct DbInfo
    {
        std::string _name;
        std::string _connectionInfo;
        ClientType _dbType;
        bool _isFast;
        size_t _connectionNumber;
    };
    std::vector<DbInfo> _dbInfos;
    std::map<std::string, IOThreadStorage<orm::DbClientPtr>> _dbFastClientsMap;
};
}  // namespace orm
}  // namespace drogon
