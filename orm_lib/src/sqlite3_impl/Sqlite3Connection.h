/**
 *
 *  Sqlite3Connection.h
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

#include "../DbConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/SerialTaskQueue.h>
#include <trantor/net/EventLoopThread.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include <thread>

namespace drogon
{
namespace orm
{

class Sqlite3Connection;
typedef std::shared_ptr<Sqlite3Connection> Sqlite3ConnectionPtr;
class Sqlite3Connection : public DbConnection, public std::enable_shared_from_this<Sqlite3Connection>
{
  public:
    Sqlite3Connection(trantor::EventLoop *loop, const std::string &connInfo);

    virtual void execSql(const std::string &sql,
                         size_t paraNum,
                         const std::vector<const char *> &parameters,
                         const std::vector<int> &length,
                         const std::vector<int> &format,
                         const ResultCallback &rcb,
                         const std::function<void(const std::exception_ptr &)> &exceptCallback,
                         const std::function<void()> &idleCb) override;

  private:
    static std::once_flag _once;
    void execSqlInQueue(const std::string &sql,
                        size_t paraNum,
                        const std::vector<const char *> &parameters,
                        const std::vector<int> &length,
                        const std::vector<int> &format,
                        const ResultCallback &rcb,
                        const std::function<void(const std::exception_ptr &)> &exceptCallback,
                        const std::function<void()> &idleCb);
    void onError(const std::string &sql, const std::function<void(const std::exception_ptr &)> &exceptCallback);
    trantor::EventLoopThread _loopThread;
    std::shared_ptr<sqlite3> _conn;
};

} // namespace orm
} // namespace drogon