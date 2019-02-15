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
    TransactionImpl(ClientType type, const DbConnectionPtr &connPtr, const std::function<void(bool)> &commitCallback, const std::function<void()> &usedUpCallback);
    ~TransactionImpl();
    void rollback() override;

  private:
    DbConnectionPtr _connectionPtr;
    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback) override;
    virtual std::shared_ptr<Transaction> newTransaction(const std::function<void(bool)>&) override
    {
        return shared_from_this();
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
        std::function<void()> _idleCb;
    };
    std::list<SqlCmd> _sqlCmdBuffer;
    //   std::mutex _bufferMutex;
    friend class DbClientImpl;
    void doBegin();
    trantor::EventLoop *_loop;
    std::function<void(bool)> _commitCallback;
};
} // namespace orm
} // namespace drogon
