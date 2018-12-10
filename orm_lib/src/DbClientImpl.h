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
#include <trantor/net/EventLoop.h>
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
  virtual void execSql(const std::string &sql,
                       size_t paraNum,
                       const std::vector<const char *> &parameters,
                       const std::vector<int> &length,
                       const std::vector<int> &format,
                       const ResultCallback &rcb,
                       const std::function<void(const std::exception_ptr &)> &exceptCallback) override;
  virtual std::shared_ptr<Transaction> newTransaction(const std::function<void(bool)> &commitCallback = std::function<void(bool)>()) override;

private:
  void ioLoop();
  std::shared_ptr<trantor::EventLoop> _loopPtr;
  void execSql(const DbConnectionPtr &conn, const std::string &sql,
               size_t paraNum,
               const std::vector<const char *> &parameters,
               const std::vector<int> &length,
               const std::vector<int> &format,
               const ResultCallback &rcb,
               const std::function<void(const std::exception_ptr &)> &exceptCallback);

  DbConnectionPtr newConnection();

  std::unordered_set<DbConnectionPtr> _connections;
  std::unordered_set<DbConnectionPtr> _readyConnections;
  std::unordered_set<DbConnectionPtr> _busyConnections;
  std::string _connInfo;
  std::thread _loopThread;
  std::mutex _connectionsMutex;
  std::condition_variable _condConnectionReady;
  size_t _transWaitNum = 0;

  size_t _connectNum;
  bool _stop = false;

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
  std::list<SqlCmd> _sqlCmdBuffer;
  std::mutex _bufferMutex;

  void handleNewTask(const DbConnectionPtr &conn);
};

} // namespace orm
} // namespace drogon
