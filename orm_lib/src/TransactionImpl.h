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
class TransactionImpl : public Transaction, public std::enable_shared_from_this<TransactionImpl>
{
  public:
    TransactionImpl(ClientType type,
                    const DbConnectionPtr &connPtr,
                    const std::function<void(bool)> &commitCallback,
                    const std::function<void()> &usedUpCallback);
    ~TransactionImpl();
    void rollback() override;
    virtual void setCommitCallback(const std::function<void(bool)> &commitCallback) override
    {
        _commitCallback = commitCallback;
    }

  private:
    DbConnectionPtr _connectionPtr;
    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback) override
    {
        if (_loop->isInLoopThread())
        {
            execSqlInLoop(std::move(sql),
                          paraNum,
                          std::move(parameters),
                          std::move(length),
                          std::move(format),
                          std::move(rcb),
                          std::move(exceptCallback));
        }
        else
        {
            _loop->queueInLoop([
                thisPtr = shared_from_this(),
                sql = std::move(sql),
                paraNum,
                parameters = std::move(parameters),
                length = std::move(length),
                format = std::move(format),
                rcb = std::move(rcb),
                exceptCallback = std::move(exceptCallback)
            ]() mutable {
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

    void execSqlInLoop(std::string &&sql,
                       size_t paraNum,
                       std::vector<const char *> &&parameters,
                       std::vector<int> &&length,
                       std::vector<int> &&format,
                       ResultCallback &&rcb,
                       std::function<void(const std::exception_ptr &)> &&exceptCallback);
    virtual std::shared_ptr<Transaction> newTransaction(const std::function<void(bool)> &) override
    {
        return shared_from_this();
    }

    virtual void newTransactionAsync(const std::function<void(const std::shared_ptr<Transaction> &)> &callback) override
    {
        callback(shared_from_this());
    }
    std::function<void()> _usedUpCallback;
    bool _isCommitedOrRolledback = false;
    bool _isWorking = false;
    void execNewTask();
    struct SqlCmd
    {
        std::string _sql;
        size_t _paraNum;
        std::vector<const char *> _parameters;
        std::vector<int> _length;
        std::vector<int> _format;
        QueryCallback _cb;
        ExceptPtrCallback _exceptCb;
        bool _isRollbackCmd = false;
        std::shared_ptr<TransactionImpl> _thisPtr;
    };
    std::list<SqlCmd> _sqlCmdBuffer;
    //   std::mutex _bufferMutex;
    friend class DbClientImpl;
    friend class DbClientLockFree;
    void doBegin();
    trantor::EventLoop *_loop;
    std::function<void(bool)> _commitCallback;
    std::shared_ptr<TransactionImpl> _thisPtr;
};
}  // namespace orm
}  // namespace drogon
