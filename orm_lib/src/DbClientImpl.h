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
    virtual std::shared_ptr<Transaction> newTransaction(const std::function<void(bool)> &commitCallback = std::function<void(bool)>()) override;

  private:
    std::string _connInfo;
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

    std::mutex _connectionsMutex;
    std::unordered_set<DbConnectionPtr> _connections;
    std::unordered_set<DbConnectionPtr> _readyConnections;
    std::unordered_set<DbConnectionPtr> _busyConnections;

    std::condition_variable _condConnectionReady;
    size_t _transWaitNum = 0;

    struct SqlCmd
    {
        std::string _sql;
        size_t _paraNum;
        std::vector<const char *> _parameters;
        std::vector<int> _length;
        std::vector<int> _format;
        QueryCallback _cb;
        ExceptPtrCallback _exceptCb;
    };
    std::deque<std::shared_ptr<SqlCmd>> _sqlCmdBuffer;
    std::mutex _bufferMutex;

    void handleNewTask(const DbConnectionPtr &conn);
};

} // namespace orm
} // namespace drogon
