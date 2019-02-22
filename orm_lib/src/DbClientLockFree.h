/**
 *
 *  DbClientLockFree.h
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

class DbClientLockFree : public DbClient, public std::enable_shared_from_this<DbClientLockFree>
{
  public:
    DbClientLockFree(const std::string &connInfo, trantor::EventLoop *loop, ClientType type);
    virtual ~DbClientLockFree() noexcept;
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
    trantor::EventLoop *_loop;
    DbConnectionPtr newConnection();
    DbConnectionPtr _connection;
    DbConnectionPtr _connectionHolder;
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
    std::deque<SqlCmd> _sqlCmdBuffer;

    void handleNewTask();
};

} // namespace orm
} // namespace drogon
