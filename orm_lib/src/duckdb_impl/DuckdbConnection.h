/**
 *
 *  @file DuckdbConnection.h
 *  @author Dq Wei
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
#include "DuckdbResultImpl.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/SerialTaskQueue.h>
#include <duckdb.h>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <set>
#include <unordered_map>

namespace drogon
{
namespace orm
{
class DuckdbConnection;
using DuckdbConnectionPtr = std::shared_ptr<DuckdbConnection>;

class DuckdbConnection : public DbConnection,
                         public std::enable_shared_from_this<DuckdbConnection>
{
  public:
    DuckdbConnection(trantor::EventLoop *loop,
                     const std::string &connInfo,
                     const std::shared_ptr<SharedMutex> &sharedMutex,
                     const std::unordered_map<std::string, std::string> &configOptions = {});  // 新增配置参数支持 [dq 2025-11-19]

    void init() override;

    void execSql(std::string_view &&sql,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)>
                     &&exceptCallback) override;

    void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) override;

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
        const char *errorMessage);

    std::shared_ptr<DuckdbResultImpl> stmtExecute(
        duckdb_prepared_statement stmt);

    // 批量 SQL 执行辅助方法 [dq 2025-11-21]
    void executeBatchSql();
    void executeSingleBatchCommand(const std::shared_ptr<SqlCmd> &cmd);

    trantor::EventLoopThread loopThread_;

    // DuckDB 数据库和连接句柄
    std::shared_ptr<duckdb_database> databasePtr_;
    std::shared_ptr<duckdb_connection> connectionPtr_;

    // 读写锁（所有连接共享）
    std::shared_ptr<SharedMutex> sharedMutexPtr_;

    // 预编译语句缓存
    std::unordered_map<std::string_view, std::shared_ptr<duckdb_prepared_statement>>
        stmtsMap_;
    std::set<std::string> stmts_;  // 维护SQL字符串的生命周期

    std::string connInfo_;
    std::unordered_map<std::string, std::string> configOptions_;  // DuckDB配置选项 [dq 2025-11-19]

    // 批量 SQL 命令队列 [dq 2025-11-21]
    std::deque<std::shared_ptr<SqlCmd>> batchSqlCommands_;
};

}  // namespace orm
}  // namespace drogon
