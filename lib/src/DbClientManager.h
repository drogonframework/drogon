/**
 *
 *  @file DbClientManager.h
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
        assert(dbClientsMap_.find(name) != dbClientsMap_.end());
        return dbClientsMap_[name];
    }
    ~DbClientManager();
    DbClientPtr getFastDbClient(const std::string &name)
    {
        auto iter = dbFastClientsMap_.find(name);
        assert(iter != dbFastClientsMap_.end());
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
                        const bool isFast,
                        const std::string &characterSet,
                        double timeout,
                        const bool autoBatch);
    bool areAllDbClientsAvailable() const noexcept;

  private:
    std::map<std::string, DbClientPtr> dbClientsMap_;
    struct DbInfo
    {
        std::string name_;
        std::string connectionInfo_;
        ClientType dbType_;
        bool isFast_;
        size_t connectionNumber_;
        double timeout_;
        bool autoBatch_;
    };
    std::vector<DbInfo> dbInfos_;
    std::map<std::string, IOThreadStorage<orm::DbClientPtr>> dbFastClientsMap_;
};
}  // namespace orm
}  // namespace drogon
