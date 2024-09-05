/**
 *
 *  @file Sqlite3Connection.h
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

#include "../DbConnection.h"
#include "Sqlite3ResultImpl.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/SerialTaskQueue.h>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <set>

namespace drogon
{
namespace orm
{
class Sqlite3Connection;
using Sqlite3ConnectionPtr = std::shared_ptr<Sqlite3Connection>;

class Sqlite3Connection : public DbConnection,
                          public std::enable_shared_from_this<Sqlite3Connection>
{
  public:
    Sqlite3Connection(trantor::EventLoop *loop,
                      const std::string &connInfo,
                      const std::shared_ptr<SharedMutex> &sharedMutex);

    void init() override;

    void execSql(std::string_view &&sql,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)>
                     &&exceptCallback) override;

    void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&) override
    {
        LOG_FATAL << "The mysql library does not support batch mode";
        exit(1);
    }

    void disconnect() override;

  private:
    static std::once_flag once_;
    void execSqlInQueue(
        const std::string_view &sql,
        size_t paraNum,
        const std::vector<const char *> &parameters,
        const std::vector<int> &length,
        const std::vector<int> &format,
        const ResultCallback &rcb,
        const std::function<void(const std::exception_ptr &)> &exceptCallback);
    void onError(
        const std::string_view &sql,
        const std::function<void(const std::exception_ptr &)> &exceptCallback,
        const int &extendedErrcode);
    int stmtStep(sqlite3_stmt *stmt,
                 const std::shared_ptr<Sqlite3ResultImpl> &resultPtr,
                 int columnNum);
    trantor::EventLoopThread loopThread_;
    std::shared_ptr<sqlite3> connectionPtr_;
    std::shared_ptr<SharedMutex> sharedMutexPtr_;
    std::unordered_map<std::string_view, std::shared_ptr<sqlite3_stmt>>
        stmtsMap_;
    std::set<std::string> stmts_;
    std::string connInfo_;
};

}  // namespace orm
}  // namespace drogon
