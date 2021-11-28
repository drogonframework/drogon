/**
 *
 *  MysqlConnection.h
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
#include <functional>
#include <iostream>
#include <memory>
#include <mysql.h>
#include <string>

namespace drogon
{
namespace orm
{
class MysqlConnection;
using MysqlConnectionPtr = std::shared_ptr<MysqlConnection>;
class MysqlConnection : public DbConnection,
                        public std::enable_shared_from_this<MysqlConnection>
{
  public:
    MysqlConnection(trantor::EventLoop *loop, const std::string &connInfo);
    ~MysqlConnection()
    {
    }
    virtual void execSql(string_view &&sql,
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
    virtual void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&) override
    {
        LOG_FATAL << "The mysql library does not support batch mode";
        exit(1);
    }
    virtual void disconnect() override;

  private:
    void execSqlInLoop(
        string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    void startSetCharacterSet();
    void continueSetCharacterSet(int status);
    std::unique_ptr<trantor::Channel> channelPtr_;
    std::shared_ptr<MYSQL> mysqlPtr_;
    std::string characterSet_;
    void handleTimeout();
    void handleCmd(int status);
    void handleClosed();
    void handleEvent();
    void setChannel();
    void getResult(MYSQL_RES *res);
    void startQuery();
    void startStoreResult(bool queueInLoop);
    int waitStatus_;
    enum class ExecStatus
    {
        None = 0,
        RealQuery,
        StoreResult,
        NextResult
    };
    ExecStatus execStatus_{ExecStatus::None};

    void outputError();
    std::string sql_;
    std::string host_, user_, passwd_, dbname_, port_;
};

}  // namespace orm
}  // namespace drogon
