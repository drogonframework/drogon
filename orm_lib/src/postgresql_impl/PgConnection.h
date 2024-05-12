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
#include <trantor/net/Channel.h>
#include <trantor/utils/NonCopyable.h>
#include <libpq-fe.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include <list>
#include <set>

namespace drogon
{
namespace orm
{
class PgConnection;
using PgConnectionPtr = std::shared_ptr<PgConnection>;

class PgConnection : public DbConnection,
                     public std::enable_shared_from_this<PgConnection>
{
  public:
    using MessageCallback =
        std::function<void(const std::string &, const std::string &)>;
    PgConnection(trantor::EventLoop *loop,
                 const std::string &connInfo,
                 bool autoBatch);

    void init() override;

    void execSql(std::string_view &&sql,
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
            loop_->queueInLoop(
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

    void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) override;

    void disconnect() override;

    const std::shared_ptr<PGconn> &pgConn() const
    {
        return connectionPtr_;
    }

    void setMessageCallback(MessageCallback cb)
    {
        messageCallback_ = std::move(cb);
    }

  private:
    std::shared_ptr<PGconn> connectionPtr_;
    trantor::Channel channel_;
    bool isPreparingStatement_{false};
    size_t preparedStatementsID_{0};

    std::string newStmtName()
    {
        loop_->assertInLoopThread();
        return std::to_string(++preparedStatementsID_);
    }

    void handleRead();
    void pgPoll();
    void handleClosed();

    void execSqlInLoop(
        std::string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    void doAfterPreparing();
    std::string statementName_;
    int parametersNumber_{0};
    std::vector<const char *> parameters_;
    std::vector<int> lengths_;
    std::vector<int> formats_;
    int flush();
    void handleFatalError();
    std::set<std::string> preparedStatements_;
    std::string_view sql_;
#if LIBPQ_SUPPORTS_BATCH_MODE
    void handleFatalError(bool clearAll, bool isAbortPipeline = false);
    std::list<std::shared_ptr<SqlCmd>> batchCommandsForWaitingResults_;
    std::deque<std::shared_ptr<SqlCmd>> batchSqlCommands_;
    void sendBatchedSql();
    int sendBatchEnd();
    bool sendBatchEnd_{false};
    bool autoBatch_{false};
    unsigned int batchCount_{0};
    std::unordered_map<std::string_view, std::pair<std::string, bool>>
        preparedStatementsMap_;
#else
    std::unordered_map<std::string_view, std::string> preparedStatementsMap_;
#endif

    MessageCallback messageCallback_;
};

}  // namespace orm
}  // namespace drogon
