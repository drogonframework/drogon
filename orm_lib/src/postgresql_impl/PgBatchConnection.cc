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
#include <memory>
#include <algorithm>
#include <stdio.h>

using namespace drogon::orm;

namespace drogon
{
namespace orm
{
static const unsigned int maxBatchCount = 256;
Result makeResult(
    const std::shared_ptr<PGresult> &r = std::shared_ptr<PGresult>(nullptr))
{
    return Result(
        std::shared_ptr<PostgreSQLResultImpl>(new PostgreSQLResultImpl(r)));
}

bool checkSql(const string_view &sql_)
{
    if (sql_.length() > 1024)
        return true;
    std::string sql{sql_.data(), sql_.length()};
    std::transform(sql.begin(), sql.end(), sql.begin(), tolower);
    return (sql.find("update") != std::string::npos ||
            sql.find("into") != std::string::npos ||
            sql.find("delete") != std::string::npos ||
            sql.find("drop") != std::string::npos ||
            sql.find("truncate") != std::string::npos ||
            sql.find("lock") != std::string::npos ||
            sql.find("create") != std::string::npos ||
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
                           const std::string &connInfo)
    : DbConnection(loop),
      connectionPtr_(
          std::shared_ptr<PGconn>(PQconnectStart(connInfo.c_str()),
                                  [](PGconn *conn) { PQfinish(conn); })),
      channel_(loop, PQsocket(connectionPtr_.get()))
{
    PQsetnonblocking(connectionPtr_.get(), 1);
    if (channel_.fd() < 0)
    {
        LOG_FATAL << "Socket fd < 0, Usually this is because the number of "
                     "files opened by the program exceeds the system "
                     "limit. Please use the ulimit command to check.";
        exit(1);
    }
    channel_.setReadCallback([this]() {
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
        thisPtr->channel_.disableAll();
        thisPtr->channel_.remove();
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
                if (!PQbeginBatchMode(connectionPtr_.get()))
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
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    LOG_TRACE << sql;
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
    if (!PQsendEndBatch(connectionPtr_.get()))
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
            if (flush())
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
                cmd->isChanging_ = checkSql(cmd->sql_);
                if (flush())
                {
                    return;
                }
            }
            else
            {
                statName = iter->second.first;
                cmd->isChanging_ = iter->second.second;
            }
        }
        else
        {
            statName = cmd->preparingStatement_;
        }
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
        if (flush())
        {
            return;
        }
        if (sendBatchEnd_)
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
        res = std::shared_ptr<PGresult>(PQgetResult(connectionPtr_.get()),
                                        [](PGresult *p) { PQclear(p); });
        if (!res)
        {
            /*
             * No more results from this query, advance to
             * the next result
             */
            if (!PQgetNextQuery(connectionPtr_.get()))
            {
                return;
            }
            continue;
        }
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR ||
            type == PGRES_BATCH_ABORTED)
        {
            handleFatalError(false);
            continue;
        }
        if (type == PGRES_BATCH_END)
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
                preparedStatementsMap_[string_view{r.first->c_str(),
                                                   r.first->length()}] = {
                    std::move(cmd->preparingStatement_), cmd->isChanging_};
                cmd->preparingStatement_.clear();
                continue;
            }
            auto r = makeResult(res);
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
            preparedStatementsMap_[string_view{r.first->c_str(),
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

void PgConnection::handleFatalError(bool clearAll)
{
    LOG_ERROR << PQerrorMessage(connectionPtr_.get());
    try
    {
        throw Failure(PQerrorMessage(connectionPtr_.get()));
    }
    catch (...)
    {
        auto exceptPtr = std::current_exception();
        if (clearAll)
        {
            for (auto &cmd : batchCommandsForWaitingResults_)
            {
                cmd->exceptionCallback_(exceptPtr);
            }
            for (auto &cmd : batchSqlCommands_)
            {
                cmd->exceptionCallback_(exceptPtr);
            }
            batchCommandsForWaitingResults_.clear();
            batchSqlCommands_.clear();
        }
        else
        {
            if (!batchSqlCommands_.empty() &&
                !batchSqlCommands_.front()->preparingStatement_.empty())
            {
                batchSqlCommands_.front()->exceptionCallback_(exceptPtr);
                batchSqlCommands_.pop_front();
            }
            else if (!batchCommandsForWaitingResults_.empty())
            {
                auto &cmd = batchCommandsForWaitingResults_.front();
                if (!cmd->preparingStatement_.empty())
                {
                    cmd->preparingStatement_.clear();
                }
                else
                {
                    cmd->exceptionCallback_(exceptPtr);
                    batchCommandsForWaitingResults_.pop_front();
                }
            }
            else
            {
                assert(false);
            }
        }
    }
}

void PgConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands)
{
    loop_->assertInLoopThread();
    batchSqlCommands_ = std::move(sqlCommands);
    sendBatchedSql();
}
