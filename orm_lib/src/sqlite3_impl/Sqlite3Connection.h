/**
 *
 *  Sqlite3Connection.h
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

#include "../DbConnection.h"
#include "Sqlite3ResultImpl.h"
#include <drogon/HttpTypes.h>
#include <drogon/orm/DbClient.h>
#include <functional>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/SerialTaskQueue.h>

namespace drogon
{
namespace orm
{
class Sqlite3Connection;
typedef std::shared_ptr<Sqlite3Connection> Sqlite3ConnectionPtr;
class Sqlite3Connection : public DbConnection, public std::enable_shared_from_this<Sqlite3Connection>
{
  public:
    Sqlite3Connection(trantor::EventLoop *loop, const std::string &connInfo, const std::shared_ptr<SharedMutex> &sharedMutex);

    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback) override;
    virtual void disconnect() override;

  private:
    static std::once_flag _once;
    void execSqlInQueue(const std::string &sql,
                        size_t paraNum,
                        const std::vector<const char *> &parameters,
                        const std::vector<int> &length,
                        const std::vector<int> &format,
                        const ResultCallback &rcb,
                        const std::function<void(const std::exception_ptr &)> &exceptCallback);
    void onError(const std::string &sql, const std::function<void(const std::exception_ptr &)> &exceptCallback);
    int stmtStep(sqlite3_stmt *stmt, const std::shared_ptr<Sqlite3ResultImpl> &resultPtr, int columnNum);
    trantor::EventLoopThread _loopThread;
    std::shared_ptr<sqlite3> _conn;
    std::shared_ptr<SharedMutex> _sharedMutexPtr;
    std::unordered_map<std::string, std::shared_ptr<sqlite3_stmt>> _stmtMap;
};

}  // namespace orm
}  // namespace drogon
