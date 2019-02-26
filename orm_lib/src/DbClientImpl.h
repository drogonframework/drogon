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
#include <drogon/HttpTypes.h>
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <unordered_set>
#include <list>

namespace drogon
{
namespace orm
{

class DbClientImpl : public DbClient, public std::enable_shared_from_this<DbClientImpl>
{
  public:
    DbClientImpl(const std::string &connInfo, const size_t connNum, ClientType type);
    virtual ~DbClientImpl() noexcept;
    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback) override;
    virtual std::shared_ptr<Transaction> newTransaction(const std::function<void(bool)> &commitCallback = nullptr) override;
    virtual void newTransactionAsync(const std::function<void(const std::shared_ptr<Transaction> &)> &callback) override;

  private:
    size_t _connectNum;
    trantor::EventLoopThreadPool _loops;
    std::shared_ptr<SharedMutex> _sharedMutexPtr;

    void execSql(const DbConnectionPtr &conn,
                 std::string &&sql,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)> &&exceptCallback);

    DbConnectionPtr newConnection(trantor::EventLoop *loop);

    void makeTrans(const DbConnectionPtr &conn, std::function<void(const std::shared_ptr<Transaction> &)> &&callback);
    
    std::mutex _connectionsMutex;
    std::unordered_set<DbConnectionPtr> _connections;
    std::unordered_set<DbConnectionPtr> _readyConnections;
    std::unordered_set<DbConnectionPtr> _busyConnections;

    std::mutex _transMutex;
    std::queue<std::function<void(const std::shared_ptr<Transaction> &)>> _transCallbacks;

    struct SqlCmd
    {
        std::string _sql;
        size_t _paraNum;
        std::vector<const char *> _parameters;
        std::vector<int> _length;
        std::vector<int> _format;
        QueryCallback _cb;
        ExceptPtrCallback _exceptCb;
        SqlCmd(std::string &&sql,
               const size_t paraNum,
               std::vector<const char *> &&parameters,
               std::vector<int> &&length,
               std::vector<int> format,
               QueryCallback &&cb,
               ExceptPtrCallback &&exceptCb)
            : _sql(std::move(sql)),
              _paraNum(paraNum),
              _parameters(std::move(parameters)),
              _length(std::move(length)),
              _format(std::move(format)),
              _cb(std::move(cb)),
              _exceptCb(std::move(exceptCb))
        {
        }
    };
    std::deque<std::shared_ptr<SqlCmd>> _sqlCmdBuffer;
    std::mutex _bufferMutex;

    void handleNewTask(const DbConnectionPtr &conn);
};

} // namespace orm
} // namespace drogon
