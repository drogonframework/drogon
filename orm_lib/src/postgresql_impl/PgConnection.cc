/**
 *
 *  @file PgConnection.cc
 *  @author An Tao
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
#include <string_view>
#include <trantor/utils/Logger.h>
#include <memory>
#include <stdio.h>

using namespace drogon;
using namespace drogon::orm;

namespace drogon
{
namespace orm
{
Result makeResult(std::shared_ptr<PGresult> &&r = nullptr)
{
    return Result(std::make_shared<PostgreSQLResultImpl>(std::move(r)));
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
                           bool)
    : DbConnection(loop),
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
                channel_.disableWriting();
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

    if (isWorking_)
    {
        // Connection was closed unexpectedly while isWorking_ was true.
        isWorking_ = false;
        handleFatalError();
        callback_ = nullptr;
    }

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
    loop_->assertInLoopThread();
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(!isWorking_);
    assert(!sql.empty());
    if (status_ != ConnectStatus::Ok)
    {
        LOG_ERROR << "Connection is not ready";
        auto exceptPtr =
            std::make_exception_ptr(drogon::orm::BrokenConnection());
        exceptCallback(exceptPtr);
        return;
    }
    sql_ = std::move(sql);
    callback_ = std::move(rcb);
    isWorking_ = true;
    exceptionCallback_ = std::move(exceptCallback);
    if (paraNum == 0)
    {
        isPreparingStatement_ = false;
        if (PQsendQuery(connectionPtr_.get(), sql_.data()) == 0)
        {
            LOG_ERROR << "send query error: "
                      << PQerrorMessage(connectionPtr_.get());
            if (isWorking_)
            {
                isWorking_ = false;
                isPreparingStatement_ = false;
                handleFatalError();
                callback_ = nullptr;
                idleCb_();
            }
            return;
        }
        flush();
    }
    else
    {
        auto iter = preparedStatementsMap_.find(sql_);
        if (iter != preparedStatementsMap_.end())
        {
            isPreparingStatement_ = false;
            if (PQsendQueryPrepared(connectionPtr_.get(),
                                    iter->second.c_str(),
                                    static_cast<int>(paraNum),
                                    parameters.data(),
                                    length.data(),
                                    format.data(),
                                    0) == 0)
            {
                LOG_ERROR << "send query error: "
                          << PQerrorMessage(connectionPtr_.get());
                if (isWorking_)
                {
                    isWorking_ = false;
                    isPreparingStatement_ = false;
                    handleFatalError();
                    callback_ = nullptr;
                    idleCb_();
                }
                return;
            }
        }
        else
        {
            isPreparingStatement_ = true;
            statementName_ = newStmtName();
            if (PQsendPrepare(connectionPtr_.get(),
                              statementName_.c_str(),
                              sql_.data(),
                              static_cast<int>(paraNum),
                              nullptr) == 0)
            {
                LOG_ERROR << "send query error: "
                          << PQerrorMessage(connectionPtr_.get());
                if (isWorking_)
                {
                    isWorking_ = false;
                    handleFatalError();
                    callback_ = nullptr;
                    idleCb_();
                }
                return;
            }
            parametersNumber_ = static_cast<int>(paraNum);
            parameters_ = std::move(parameters);
            lengths_ = std::move(length);
            formats_ = std::move(format);
        }
        flush();
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
            handleFatalError();
            callback_ = nullptr;
        }
        handleClosed();
        return;
    }
    if (PQisBusy(connectionPtr_.get()))
    {
        // need read more data from socket;
        return;
    }
    while ((res = std::shared_ptr<PGresult>(PQgetResult(connectionPtr_.get()),
                                            [](PGresult *p) { PQclear(p); })))
    {
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR)
        {
            LOG_WARN << PQerrorMessage(connectionPtr_.get());
            if (isWorking_)
            {
                handleFatalError();
                callback_ = nullptr;
            }
        }
        else
        {
            if (isWorking_)
            {
                if (!isPreparingStatement_)
                {
                    auto r = makeResult(std::move(res));
                    callback_(r);
                    callback_ = nullptr;
                    exceptionCallback_ = nullptr;
                }
            }
        }
    }
    if (isWorking_)
    {
        if (isPreparingStatement_ && callback_)
        {
            doAfterPreparing();
        }
        else
        {
            isWorking_ = false;
            isPreparingStatement_ = false;
            idleCb_();
        }
    }

    // Check notification
    std::shared_ptr<PGnotify> notify;
    while (
        (notify = std::shared_ptr<PGnotify>(PQnotifies(connectionPtr_.get()),
                                            [](PGnotify *p) { PQfreemem(p); })))
    {
        messageCallback_({notify->relname}, {notify->extra});
    }
}

void PgConnection::doAfterPreparing()
{
    isPreparingStatement_ = false;
    auto r = preparedStatements_.insert(std::string{sql_});
    preparedStatementsMap_[std::string_view{r.first->data(),
                                            r.first->length()}] =
        statementName_;
    if (PQsendQueryPrepared(connectionPtr_.get(),
                            statementName_.c_str(),
                            parametersNumber_,
                            parameters_.data(),
                            lengths_.data(),
                            formats_.data(),
                            0) == 0)
    {
        LOG_ERROR << "send query error: "
                  << PQerrorMessage(connectionPtr_.get());
        if (isWorking_)
        {
            isWorking_ = false;
            handleFatalError();
            callback_ = nullptr;
            idleCb_();
        }
        return;
    }
    flush();
}

void PgConnection::handleFatalError()
{
    if (exceptionCallback_)
    {
        auto exceptPtr = std::make_exception_ptr(
            Failure(PQerrorMessage(connectionPtr_.get())));
        exceptionCallback_(exceptPtr);
    }

    exceptionCallback_ = nullptr;
}

void PgConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&)
{
    assert(false);
}
