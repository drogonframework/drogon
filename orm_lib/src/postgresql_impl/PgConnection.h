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
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <trantor/utils/NonCopyable.h>
#include <libpq-fe.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include <list>

namespace drogon
{
namespace orm
{
class PgConnection;
typedef std::shared_ptr<PgConnection> PgConnectionPtr;
class PgConnection : public DbConnection,
                     public std::enable_shared_from_this<PgConnection>
{
  public:
    PgConnection(trantor::EventLoop *loop, const std::string &connInfo);

    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)>
                             &&exceptCallback) override
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
            auto thisPtr = shared_from_this();
            _loop->queueInLoop(
                [thisPtr,
                 sql = std::move(sql),
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

    virtual void batchSql(
        std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) override;

    virtual void disconnect() override;

  private:
    std::shared_ptr<PGconn> _connPtr;
    trantor::Channel _channel;
    std::unordered_map<std::string, std::string> _preparedStatementMap;
    bool _isRreparingStatement = false;
    void handleRead();
    void pgPoll();
    void handleClosed();

    void execSqlInLoop(
        std::string &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    void doAfterPreparing();
    std::string _statementName;
    int _paraNum;
    std::vector<const char *> _parameters;
    std::vector<int> _length;
    std::vector<int> _format;
    int flush();
    void handleFatalError();
#if LIBPQ_SUPPORTS_BATCH_MODE
    std::list<std::shared_ptr<SqlCmd>> _batchCommandsForWaitingResults;
    std::deque<std::shared_ptr<SqlCmd>> _batchSqlCommands;
    void sendBatchedSql();
    int sendBatchEnd();
    bool _sendBatchEnd = false;
    unsigned int _batchCount = 0;
#endif
};

}  // namespace orm
}  // namespace drogon
