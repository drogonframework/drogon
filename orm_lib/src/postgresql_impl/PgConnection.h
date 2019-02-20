/**
 *
 *  PgConnection.h
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
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/NonCopyable.h>
#include <libpq-fe.h>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace drogon
{
namespace orm
{

class PgConnection;
typedef std::shared_ptr<PgConnection> PgConnectionPtr;
class PgConnection : public DbConnection, public std::enable_shared_from_this<PgConnection>
{
  public:
    PgConnection(trantor::EventLoop *loop, const std::string &connInfo);

    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback,
                         std::function<void()> &&idleCb) override
    {
        if (_loop->isInLoopThread())
        {
            execSqlInLoop(std::move(sql),
                          paraNum,
                          std::move(parameters),
                          std::move(length),
                          std::move(format),
                          std::move(rcb),
                          std::move(exceptCallback),
                          std::move(idleCb));
        }
        else
        {
            auto thisPtr = shared_from_this();
            _loop->queueInLoop([thisPtr,
                                sql = std::move(sql),
                                paraNum,
                                parameters = std::move(parameters),
                                length = std::move(length),
                                format = std::move(format),
                                rcb = std::move(rcb),
                                exceptCallback = std::move(exceptCallback),
                                idleCb = std::move(idleCb)]() mutable {
                thisPtr->execSqlInLoop(std::move(sql),
                                       paraNum,
                                       std::move(parameters),
                                       std::move(length),
                                       std::move(format),
                                       std::move(rcb),
                                       std::move(exceptCallback),
                                       std::move(idleCb));
            });
        }
    }
    virtual void disconnect() override;

  private:
    std::shared_ptr<PGconn> _connPtr;
    trantor::Channel _channel;
    std::unordered_map<std::string, std::string> _preparedStatementMap;
    bool _isRreparingStatement = false;
    void handleRead();
    void pgPoll();
    void handleClosed();

    void execSqlInLoop(std::string &&sql,
                       size_t paraNum,
                       std::vector<const char *> &&parameters,
                       std::vector<int> &&length,
                       std::vector<int> &&format,
                       ResultCallback &&rcb,
                       std::function<void(const std::exception_ptr &)> &&exceptCallback,
                       std::function<void()> &&idleCb);
    std::function<void()> _preparingCallback;
};

} // namespace orm
} // namespace drogon
