/**
 *
 *  PgConnection.cc
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

#include "PgConnection.h"
#include "PostgreSQLResultImpl.h"
#include <drogon/orm/Exception.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <exception>
#include <memory>
#include <algorithm>
#include <stdio.h>

using namespace drogon::orm;

namespace drogon
{
namespace orm
{
static const unsigned int maxBatchCount = 256;

Result makeResult(std::shared_ptr<PGresult> &&r = nullptr)
{
    return Result(std::make_shared<PostgreSQLResultImpl>(std::move(r)));
}

bool checkSql(const std::string_view &sql_)
{
    if (sql_.length() > 1024)
        return true;
    std::string sql{sql_.data(), sql_.length()};
    std::transform(sql.begin(), sql.end(), sql.begin(), [](unsigned char c) {
        return tolower(c);
    });
    return (sql.find("update") != std::string::npos ||
            sql.find("into") != std::string::npos ||
            sql.find("delete") != std::string::npos ||
            sql.find("drop") != std::string::npos ||
            sql.find("truncate") != std::string::npos ||
            sql.find("lock") != std::string::npos ||
            sql.find("create") != std::string::npos ||
            sql.find("call") != std::string::npos ||
            sql.find("alter") != std::string::npos);
}

}  // namespace orm
}  // namespace drogon

int PgConnection::flush()
{
    auto ret = PQflush(connectionPtr_.get());
    if (ret == 1)
    {
        if (!channel_.isWriting())
        {
            channel_.enableWriting();
        }
    }
    else if (ret == 0)
    {
        if (channel_.isWriting())
        {
            channel_.disableWriting();
        }
    }
    return ret;
}

PgConnection::PgConnection(trantor::EventLoop *loop,
                           const std::string &connInfo,
                           bool autoBatch)
    : DbConnection(loop),
      autoBatch_(autoBatch),
      connectionPtr_(
          std::shared_ptr<PGconn>(PQconnectStart(connInfo.c_str()),
                                  [](PGconn *conn) { PQfinish(conn); })),
      channel_(loop, PQsocket(connectionPtr_.get()))
{
    if (channel_.fd() < 0)
    {
        LOG_ERROR << "Failed to create Postgres connection";
    }
}

void PgConnection::init()
{
    if (channel_.fd() < 0)
    {
        LOG_ERROR << "Connection with Postgres could not be established";

        if (closeCallback_)
        {
            auto thisPtr = shared_from_this();
            closeCallback_(thisPtr);
        }
        return;
    }

    PQsetnonblocking(connectionPtr_.get(), 1);
    channel_.setReadCallback([this]() {
        if (status_ == ConnectStatus::Bad)
        {
            return;
        }
        if (status_ != ConnectStatus::Ok)
        {
            pgPoll();
        }
        else
        {
            handleRead();
        }
    });
    channel_.setWriteCallback([this]() {
        if (status_ == ConnectStatus::Ok)
        {
            auto ret = PQflush(connectionPtr_.get());
            if (ret == 0)
            {
                sendBatchedSql();
                return;
            }
            else if (ret < 0)
            {
                channel_.disableWriting();
                LOG_ERROR << "PQflush error:"
                          << PQerrorMessage(connectionPtr_.get());
                return;
            }
        }
        else
        {
            pgPoll();
        }
    });
    channel_.setCloseCallback([this]() { handleClosed(); });
    channel_.setErrorCallback([this]() { handleClosed(); });
    channel_.enableReading();
    channel_.enableWriting();
}

void PgConnection::handleClosed()
{
    loop_->assertInLoopThread();
    if (status_ == ConnectStatus::Bad)
        return;
    status_ = ConnectStatus::Bad;
    channel_.disableAll();
    channel_.remove();
    assert(closeCallback_);
    auto thisPtr = shared_from_this();
    closeCallback_(thisPtr);
}

void PgConnection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr, &pro]() {
        thisPtr->status_ = ConnectStatus::Bad;
        if (thisPtr->channel_.fd() >= 0)
        {
            thisPtr->channel_.disableAll();
            thisPtr->channel_.remove();
        }
        thisPtr->connectionPtr_.reset();
        pro.set_value(1);
    });
    f.get();
}

void PgConnection::pgPoll()
{
    loop_->assertInLoopThread();
    auto connStatus = PQconnectPoll(connectionPtr_.get());
    switch (connStatus)
    {
        case PGRES_POLLING_FAILED:
            LOG_ERROR << "!!!Pg connection failed: "
                      << PQerrorMessage(connectionPtr_.get());
            if (status_ == ConnectStatus::None)
            {
                handleClosed();
            }
            break;
        case PGRES_POLLING_WRITING:
            if (!channel_.isWriting())
                channel_.enableWriting();
            break;
        case PGRES_POLLING_READING:
            if (!channel_.isReading())
                channel_.enableReading();
            if (channel_.isWriting())
                channel_.disableWriting();
            break;

        case PGRES_POLLING_OK:
            if (status_ != ConnectStatus::Ok)
            {
                status_ = ConnectStatus::Ok;
                if (!PQenterPipelineMode(connectionPtr_.get()))
                {
                    handleClosed();
                    return;
                }
                assert(okCallback_);
                okCallback_(shared_from_this());
            }
            if (!channel_.isReading())
                channel_.enableReading();
            if (channel_.isWriting())
                channel_.disableWriting();
            break;
        case PGRES_POLLING_ACTIVE:
            // unused!
            break;
        default:
            break;
    }
}

void PgConnection::execSqlInLoop(
    std::string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    LOG_TRACE << sql;
    if (status_ != ConnectStatus::Ok)
    {
        LOG_ERROR << "Connection is not ready";
        auto exceptPtr =
            std::make_exception_ptr(drogon::orm::BrokenConnection());
        exceptCallback(exceptPtr);
        return;
    }
    isWorking_ = true;
    batchSqlCommands_.emplace_back(
        std::make_shared<SqlCmd>(std::move(sql),
                                 paraNum,
                                 std::move(parameters),
                                 std::move(length),
                                 std::move(format),
                                 std::move(rcb),
                                 std::move(exceptCallback)));
    if (batchSqlCommands_.size() == 1 && !channel_.isWriting())
    {
        loop_->queueInLoop(
            [thisPtr = shared_from_this()]() { thisPtr->sendBatchedSql(); });
    }
}

int PgConnection::sendBatchEnd()
{
    if (!PQpipelineSync(connectionPtr_.get()))
    {
        isWorking_ = false;
        handleFatalError(true);
        handleClosed();
        return 0;
    }
    return 1;
}

void PgConnection::sendBatchedSql()
{
    if (isWorking_)
    {
        if (sendBatchEnd_)
        {
            sendBatchEnd_ = false;
            if (!sendBatchEnd())
            {
                return;
            }
        }
        if (batchSqlCommands_.empty())
        {
            if (channel_.isWriting())
                channel_.disableWriting();
            return;
        }
    }
    isWorking_ = true;
    while (!batchSqlCommands_.empty())
    {
        auto &cmd = batchSqlCommands_.front();
        std::string statName;
        if (cmd->preparingStatement_.empty())
        {
            auto iter = preparedStatementsMap_.find(cmd->sql_);
            if (iter == preparedStatementsMap_.end())
            {
                statName = newStmtName();
                if (PQsendPrepare(connectionPtr_.get(),
                                  statName.c_str(),
                                  cmd->sql_.data(),
                                  cmd->parametersNumber_,
                                  NULL) == 0)
                {
                    LOG_ERROR << "send query error: "
                              << PQerrorMessage(connectionPtr_.get());

                    isWorking_ = false;
                    handleFatalError(true);
                    handleClosed();
                    return;
                }
                cmd->preparingStatement_ = statName;
                if (autoBatch_)
                {
                    cmd->isChanging_ = checkSql(cmd->sql_);
                }
                if (flush())
                {
                    return;
                }
            }
            else
            {
                statName = iter->second.first;
                if (autoBatch_)
                {
                    cmd->isChanging_ = iter->second.second;
                }
            }
        }
        else
        {
            statName = cmd->preparingStatement_;
        }
        if (autoBatch_)
        {
            if (batchSqlCommands_.size() == 1 || cmd->sql_.length() > 1024 ||
                batchCount_ > maxBatchCount)
            {
                sendBatchEnd_ = true;
                batchCount_ = 0;
            }
            else if (cmd->isChanging_)
            {
                sendBatchEnd_ = true;
                batchCount_ = 0;
            }
            ++batchCount_;
        }
        if (PQsendQueryPrepared(connectionPtr_.get(),
                                statName.c_str(),
                                cmd->parametersNumber_,
                                cmd->parameters_.data(),
                                cmd->lengths_.data(),
                                cmd->formats_.data(),
                                0) == 0)
        {
            isWorking_ = false;
            handleFatalError(true);
            handleClosed();
            return;
        }

        batchCommandsForWaitingResults_.push_back(std::move(cmd));
        batchSqlCommands_.pop_front();
        if (!autoBatch_)
        {
            if (!sendBatchEnd())
            {
                return;
            }
        }
        else
        {
            if (flush())
            {
                return;
            }
        }
        if (autoBatch_ && sendBatchEnd_)
        {
            sendBatchEnd_ = false;
            if (!sendBatchEnd())
            {
                return;
            }
            if (flush())
            {
                return;
            }
        }
        else
        {
            if (flush())
            {
                return;
            }
        }
    }
}

void PgConnection::handleRead()
{
    loop_->assertInLoopThread();
    std::shared_ptr<PGresult> res;

    if (!PQconsumeInput(connectionPtr_.get()))
    {
        LOG_ERROR << "Failed to consume pg input:"
                  << PQerrorMessage(connectionPtr_.get());
        if (isWorking_)
        {
            isWorking_ = false;
            handleFatalError(true);
        }
        handleClosed();
        return;
    }
    if (PQisBusy(connectionPtr_.get()))
    {
        // need read more data from socket;
        return;
    }
    // assert((!batchCommandsForWaitingResults_.empty() ||
    //         !batchSqlCommands_.empty()));

    while (!PQisBusy(connectionPtr_.get()))
    {
        // TODO: should optimize order of checking
        // Check notification
        std::shared_ptr<PGnotify> notify;
        while (
            (notify =
                 std::shared_ptr<PGnotify>(PQnotifies(connectionPtr_.get()),
                                           [](PGnotify *p) { PQfreemem(p); })))
        {
            messageCallback_({notify->relname}, {notify->extra});
        }

        // Check query result
        res = std::shared_ptr<PGresult>(PQgetResult(connectionPtr_.get()),
                                        [](PGresult *p) { PQclear(p); });
        if (!res)
        {
            /*
             * No more results currtently available.
             */
            if (!PQsendFlushRequest(connectionPtr_.get()))
            {
                LOG_ERROR << "Failed to PQsendFlushRequest:"
                          << PQerrorMessage(connectionPtr_.get());
                return;
            }
            res = std::shared_ptr<PGresult>(PQgetResult(connectionPtr_.get()),
                                            [](PGresult *p) { PQclear(p); });
            if (!res)
            {
                return;
            }
        }
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR ||
            type == PGRES_PIPELINE_ABORTED)
        {
            handleFatalError(false, type == PGRES_PIPELINE_ABORTED);
            continue;
        }
        if (type == PGRES_PIPELINE_SYNC)
        {
            if (batchCommandsForWaitingResults_.empty() &&
                batchSqlCommands_.empty())
            {
                isWorking_ = false;
                idleCb_();
                return;
            }
            continue;
        }
        if (!batchCommandsForWaitingResults_.empty())
        {
            auto &cmd = batchCommandsForWaitingResults_.front();
            if (!cmd->preparingStatement_.empty())
            {
                auto r = preparedStatements_.insert(
                    std::string{cmd->sql_.data(), cmd->sql_.length()});
                preparedStatementsMap_[std::string_view{r.first->c_str(),
                                                        r.first->length()}] = {
                    std::move(cmd->preparingStatement_), cmd->isChanging_};
                cmd->preparingStatement_.clear();
                continue;
            }
            auto r = makeResult(std::move(res));
            cmd->callback_(r);
            batchCommandsForWaitingResults_.pop_front();
            continue;
        }
        assert(!batchSqlCommands_.empty());
        assert(!batchSqlCommands_.front()->preparingStatement_.empty());
        auto &cmd = batchSqlCommands_.front();
        if (!cmd->preparingStatement_.empty())
        {
            auto r = preparedStatements_.insert(
                std::string{cmd->sql_.data(), cmd->sql_.length()});
            preparedStatementsMap_[std::string_view{r.first->c_str(),
                                                    r.first->length()}] = {
                std::move(cmd->preparingStatement_), cmd->isChanging_};
            cmd->preparingStatement_.clear();
            continue;
        }
    }
}

void PgConnection::doAfterPreparing()
{
}

void PgConnection::handleFatalError(bool clearAll, bool isAbortPipeline)
{
    std::string errmsg =
        isAbortPipeline
            ? "Command didn't run because of an abort earlier in a pipeline"
            : PQerrorMessage(connectionPtr_.get());
    LOG_ERROR << errmsg;
    auto exceptPtr = std::make_exception_ptr(Failure(errmsg));
    if (clearAll)
    {
        for (auto &cmd : batchCommandsForWaitingResults_)
        {
            if (cmd->exceptionCallback_)
            {
                cmd->exceptionCallback_(exceptPtr);
            }
        }
        for (auto &cmd : batchSqlCommands_)
        {
            if (cmd->exceptionCallback_)
            {
                cmd->exceptionCallback_(exceptPtr);
            }
        }
        batchCommandsForWaitingResults_.clear();
        batchSqlCommands_.clear();
    }
    else
    {
        if (!batchSqlCommands_.empty() &&
            !batchSqlCommands_.front()->preparingStatement_.empty())
        {
            if (batchSqlCommands_.front()->exceptionCallback_)
            {
                batchSqlCommands_.front()->exceptionCallback_(exceptPtr);
            }
            batchSqlCommands_.pop_front();
        }
        else if (!batchCommandsForWaitingResults_.empty())
        {
            auto &cmd = batchCommandsForWaitingResults_.front();
            if (cmd->exceptionCallback_)
            {
                cmd->exceptionCallback_(exceptPtr);
            }
            batchCommandsForWaitingResults_.pop_front();
        }
        else
        {
            // PQsendPrepare failed, error message has already been reported
            // Ignore PQsendQueryPrepared failure
        }
    }
}

void PgConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands)
{
    loop_->assertInLoopThread();
    batchSqlCommands_ = std::move(sqlCommands);
    sendBatchedSql();
}
