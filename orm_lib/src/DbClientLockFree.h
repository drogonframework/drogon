/**
 *
 *  @file DbClientLockFree.h
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

#include "DbConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <list>

namespace drogon
{
namespace orm
{
class DbClientLockFree : public DbClient,
                         public std::enable_shared_from_this<DbClientLockFree>
{
  public:
    DbClientLockFree(const std::string &connInfo,
                     trantor::EventLoop *loop,
                     ClientType type,
#if LIBPQ_SUPPORTS_BATCH_MODE
                     size_t connectionNumberPerLoop,
                     bool autoBatch);
#else
                     size_t connectionNumberPerLoop);
#endif

    ~DbClientLockFree() noexcept override;
    void execSql(const char *sql,
                 size_t sqlLength,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)>
                     &&exceptCallback) override;
    std::shared_ptr<Transaction> newTransaction(
        const std::function<void(bool)> &commitCallback =
            std::function<void(bool)>()) noexcept(false) override;
    void newTransactionAsync(
        const std::function<void(const std::shared_ptr<Transaction> &)>
            &callback) override;
    bool hasAvailableConnections() const noexcept override;

    void setTimeout(double timeout) override
    {
        timeout_ = timeout;
    }

    void closeAll() override;

  private:
    std::string connectionInfo_;
    trantor::EventLoop *loop_;
    DbConnectionPtr newConnection();
    const size_t numberOfConnections_;
    std::vector<DbConnectionPtr> connections_;
    std::vector<DbConnectionPtr> connectionHolders_;
    std::unordered_set<DbConnectionPtr> transSet_;
    std::deque<std::shared_ptr<SqlCmd>> sqlCmdBuffer_;

    std::list<std::shared_ptr<
        std::function<void(const std::shared_ptr<Transaction> &)>>>
        transCallbacks_;

    double timeout_{-1.0};

    void makeTrans(
        const DbConnectionPtr &conn,
        std::function<void(const std::shared_ptr<Transaction> &)> &&callback);
    void execSqlWithTimeout(
        const char *sql,
        size_t sqlLength,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&ecb);
    void handleNewTask(const DbConnectionPtr &conn);
#if LIBPQ_SUPPORTS_BATCH_MODE
    size_t connectionPos_{0};  // Used for pg batch mode.
    bool autoBatch_{false};
#endif
};

}  // namespace orm
}  // namespace drogon
