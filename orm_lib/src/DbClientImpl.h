/**
 *
 *  DbClientImpl.h
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

#include "DbConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>

namespace drogon
{
namespace orm
{
class DbClientImpl : public DbClient,
                     public std::enable_shared_from_this<DbClientImpl>
{
  public:
    DbClientImpl(const std::string &connInfo,
                 const size_t connNum,
                 ClientType type);
    virtual ~DbClientImpl() noexcept;
    virtual void execSql(const char *sql,
                         size_t sqlLength,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)>
                             &&exceptCallback) override;
    virtual std::shared_ptr<Transaction> newTransaction(
        const std::function<void(bool)> &commitCallback = nullptr) override;
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<Transaction> &)>
            &callback) override;
    virtual bool hasAvailableConnections() const noexcept override;

  private:
    size_t numberOfConnections_;
    trantor::EventLoopThreadPool loops_;
    std::shared_ptr<SharedMutex> sharedMutexPtr_;

    void execSql(
        const DbConnectionPtr &conn,
        string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);

    DbConnectionPtr newConnection(trantor::EventLoop *loop);

    void makeTrans(
        const DbConnectionPtr &conn,
        std::function<void(const std::shared_ptr<Transaction> &)> &&callback);

    mutable std::mutex connectionsMutex_;
    std::unordered_set<DbConnectionPtr> connections_;
    std::unordered_set<DbConnectionPtr> readyConnections_;
    std::unordered_set<DbConnectionPtr> busyConnections_;

    std::queue<std::function<void(const std::shared_ptr<Transaction> &)>>
        transCallbacks_;

    std::deque<std::shared_ptr<SqlCmd>> sqlCmdBuffer_;

    void handleNewTask(const DbConnectionPtr &connPtr);
};

}  // namespace orm
}  // namespace drogon
