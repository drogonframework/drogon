/**
 *
 *  TransactionImpl.h
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
#include <functional>
#include <list>

namespace drogon
{
namespace orm
{
class TransactionImpl : public Transaction,
                        public std::enable_shared_from_this<TransactionImpl>
{
  public:
    TransactionImpl(ClientType type,
                    const DbConnectionPtr &connPtr,
                    std::function<void(bool)> commitCallback,
                    std::function<void()> usedUpCallback);
    ~TransactionImpl() override;
    void rollback() override;
    void setCommitCallback(
        const std::function<void(bool)> &commitCallback) override
    {
        commitCallback_ = commitCallback;
    }
    bool hasAvailableConnections() const noexcept override
    {
        return connectionPtr_->status() == ConnectStatus::Ok;
    }
    void setTimeout(double timeout) override
    {
        timeout_ = timeout;
    }

  private:
    DbConnectionPtr connectionPtr_;
    void execSql(const char *sql,
                 size_t sqlLength,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)>
                     &&exceptCallback) override
    {
        if (loop_->isInLoopThread())
        {
            execSqlInLoop(string_view{sql, sqlLength},
                          paraNum,
                          std::move(parameters),
                          std::move(length),
                          std::move(format),
                          std::move(rcb),
                          std::move(exceptCallback));
        }
        else
        {
            loop_->queueInLoop(
                [thisPtr = shared_from_this(),
                 sql = string_view{sql, sqlLength},
                 paraNum,
                 parameters = std::move(parameters),
                 length = std::move(length),
                 format = std::move(format),
                 rcb = std::move(rcb),
                 exceptCallback = std::move(exceptCallback)]() mutable {
                    thisPtr->execSqlInLoop(std::move(sql),
                                           paraNum,
                                           std::move(parameters),
                                           std::move(length),
                                           std::move(format),
                                           std::move(rcb),
                                           std::move(exceptCallback));
                });
        }
    }

    void execSqlInLoop(
        string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    void execSqlInLoopWithTimeout(
        string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    std::shared_ptr<Transaction> newTransaction(
        const std::function<void(bool)> &) noexcept(false) override
    {
        return shared_from_this();
    }

    void newTransactionAsync(
        const std::function<void(const std::shared_ptr<Transaction> &)>
            &callback) override
    {
        callback(shared_from_this());
    }
    std::function<void()> usedUpCallback_;
    bool isCommitedOrRolledback_{false};
    bool isWorking_{false};
    void execNewTask();
    struct SqlCmd
    {
        string_view sql_;
        size_t parametersNumber_;
        std::vector<const char *> parameters_;
        std::vector<int> lengths_;
        std::vector<int> formats_;
        QueryCallback callback_;
        ExceptPtrCallback exceptionCallback_;
        bool isRollbackCmd_{false};
        std::shared_ptr<TransactionImpl> thisPtr_;
    };
    using SqlCmdPtr = std::shared_ptr<SqlCmd>;
    std::list<SqlCmdPtr> sqlCmdBuffer_;
    //   std::mutex _bufferMutex;
    friend class DbClientImpl;
    friend class DbClientLockFree;
    void doBegin();
    trantor::EventLoop *loop_;
    std::function<void(bool)> commitCallback_;
    std::shared_ptr<TransactionImpl> thisPtr_;
    double timeout_{-1.0};
};
}  // namespace orm
}  // namespace drogon
