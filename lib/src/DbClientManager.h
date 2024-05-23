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
#include <drogon/orm/DbConfig.h>
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
    void createDbClients(const std::vector<trantor::EventLoop *> &ioLoops);

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

    void addDbClient(const DbConfig &config);
    bool areAllDbClientsAvailable() const noexcept;

  private:
    std::map<std::string, DbClientPtr> dbClientsMap_;

    struct DbInfo
    {
        std::string connectionInfo_;
        DbConfig config_;
    };

    std::vector<DbInfo> dbInfos_;
    std::map<std::string, IOThreadStorage<orm::DbClientPtr>> dbFastClientsMap_;
};
}  // namespace orm
}  // namespace drogon
