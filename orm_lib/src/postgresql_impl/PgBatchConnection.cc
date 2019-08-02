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
Result makeResult(
    const std::shared_ptr<PGresult> &r = std::shared_ptr<PGresult>(nullptr),
    const std::string &query = "")
{
    return Result(std::shared_ptr<PostgreSQLResultImpl>(
        new PostgreSQLResultImpl(r, query)));
}

}  // namespace orm
}  // namespace drogon
int PgConnection::flush()
{
    auto ret = PQflush(_connPtr.get());
    if (ret == 1)
    {
        if (!_channel.isWriting())
        {
            _channel.enableWriting();
        }
    }
    else if (ret == 0)
    {
        if (_channel.isWriting())
        {
            _channel.disableWriting();
        }
    }
    return ret;
}
PgConnection::PgConnection(trantor::EventLoop *loop,
                           const std::string &connInfo)
    : DbConnection(loop),
      _connPtr(std::shared_ptr<PGconn>(PQconnectStart(connInfo.c_str()),
                                       [](PGconn *conn) { PQfinish(conn); })),
      _channel(loop, PQsocket(_connPtr.get()))
{
    PQsetnonblocking(_connPtr.get(), 1);
    if (_channel.fd() < 0)
    {
        LOG_FATAL << "Socket fd < 0, Usually this is because the number of "
                     "files opened by the program exceeds the system "
                     "limit. Please use the ulimit command to check.";
        exit(1);
    }
    _channel.setReadCallback([=]() {
        if (_status != ConnectStatus_Ok)
        {
            pgPoll();
        }
        else
        {
            handleRead();
        }
    });
    _channel.setWriteCallback([=]() {
        if (_status == ConnectStatus_Ok)
        {
            auto ret = PQflush(_connPtr.get());
            if (ret == 0)
            {
                sendBatchedSql();
                return;
            }
            else if (ret < 0)
            {
                _channel.disableWriting();
                LOG_ERROR << "PQflush error:" << PQerrorMessage(_connPtr.get());
                return;
            }
        }
        else
        {
            pgPoll();
        }
    });
    _channel.setCloseCallback([=]() { handleClosed(); });
    _channel.setErrorCallback([=]() { handleClosed(); });
    _channel.enableReading();
    _channel.enableWriting();
}

void PgConnection::handleClosed()
{
    _loop->assertInLoopThread();
    if (_status == ConnectStatus_Bad)
        return;
    _status = ConnectStatus_Bad;
    _channel.disableAll();
    _channel.remove();
    assert(_closeCb);
    auto thisPtr = shared_from_this();
    _closeCb(thisPtr);
}

void PgConnection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    _loop->runInLoop([thisPtr, &pro]() {
        thisPtr->_status = ConnectStatus_Bad;
        thisPtr->_channel.disableAll();
        thisPtr->_channel.remove();
        thisPtr->_connPtr.reset();
        pro.set_value(1);
    });
    f.get();
}

void PgConnection::pgPoll()
{
    _loop->assertInLoopThread();
    auto connStatus = PQconnectPoll(_connPtr.get());

    switch (connStatus)
    {
        case PGRES_POLLING_FAILED:
            LOG_ERROR << "!!!Pg connection failed: "
                      << PQerrorMessage(_connPtr.get());
            if (_status == ConnectStatus_None)
            {
                handleClosed();
            }
            break;
        case PGRES_POLLING_WRITING:
            if (!_channel.isWriting())
                _channel.enableWriting();
            break;
        case PGRES_POLLING_READING:
            if (!_channel.isReading())
                _channel.enableReading();
            if (_channel.isWriting())
                _channel.disableWriting();
            break;

        case PGRES_POLLING_OK:
            if (_status != ConnectStatus_Ok)
            {
                _status = ConnectStatus_Ok;
                if (!PQbeginBatchMode(_connPtr.get()))
                {
                    handleClosed();
                    return;
                }
                assert(_okCb);
                _okCb(shared_from_this());
            }
            if (!_channel.isReading())
                _channel.enableReading();
            if (_channel.isWriting())
                _channel.disableWriting();
            break;
        case PGRES_POLLING_ACTIVE:
            // unused!
            break;
        default:
            break;
    }
}

void PgConnection::execSqlInLoop(
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    LOG_TRACE << sql;
    _isWorking = true;
    _batchSqlCommands.emplace_back(
        std::make_shared<SqlCmd>(std::move(sql),
                                 paraNum,
                                 std::move(parameters),
                                 std::move(length),
                                 std::move(format),
                                 std::move(rcb),
                                 std::move(exceptCallback)));
    if (_batchSqlCommands.size() == 1 && !_channel.isWriting())
    {
        _loop->queueInLoop([thisPtr = shared_from_this()](){
            thisPtr->sendBatchedSql();
        });
    }
}
int PgConnection::sendBatchEnd()
{
    if (!PQsendEndBatch(_connPtr.get()))
    {
        _isWorking = false;
        handleFatalError();
        handleClosed();
        return 0;
    }
    return 1;
}
void PgConnection::sendBatchedSql()
{
    if (_isWorking && _batchSqlCommands.empty())
    {
        if (_sendBatchEnd)
        {
            if (sendBatchEnd())
            {
                _sendBatchEnd = false;
                if (flush())
                {
                    return;
                }
                else
                {
                    if (_channel.isWriting())
                        _channel.disableWriting();
                }
            }
            return;
        }
        if (_channel.isWriting())
            _channel.disableWriting();
        return;
    }
    _isWorking = true;
    while (!_batchSqlCommands.empty())
    {
        auto &cmd = _batchSqlCommands.front();
        std::string statName;
        auto iter = _preparedStatementMap.find(cmd->_sql);
        if (iter == _preparedStatementMap.end())
        {
            statName = utils::getUuid();
            if (PQsendPrepare(_connPtr.get(),
                              statName.c_str(),
                              cmd->_sql.c_str(),
                              cmd->_paraNum,
                              NULL) == 0)
            {
                LOG_ERROR << "send query error: "
                          << PQerrorMessage(_connPtr.get());

                _isWorking = false;
                handleFatalError();
                handleClosed();
                return;
            }
            cmd->_preparingStatement = statName;
            if (flush())
            {
                return;
            }
        }
        else
        {
            statName = iter->second;
        }

        if (_batchSqlCommands.size() == 1)
        {
            _sendBatchEnd = true;
        }
        else
        {
            auto sql = cmd->_sql;
            std::transform(sql.begin(), sql.end(), sql.begin(), tolower);
            if (sql.find("update") != std::string::npos ||
                sql.find("insert") != std::string::npos ||
                sql.find("delete") != std::string::npos ||
                sql.find("drop") != std::string::npos ||
                sql.find("truncate") != std::string::npos ||
                sql.find("lock") != std::string::npos)
            {
                _sendBatchEnd = true;
            }
        }
        if (PQsendQueryPrepared(_connPtr.get(),
                                statName.c_str(),
                                cmd->_paraNum,
                                cmd->_parameters.data(),
                                cmd->_length.data(),
                                cmd->_format.data(),
                                0) == 0)
        {
            _isWorking = false;
            handleFatalError();
            handleClosed();
            return;
        }
        _batchCommandsForWaitingResults.push_back(std::move(cmd));
        _batchSqlCommands.pop_front();
        if (flush())
        {
            return;
        }
        if (_sendBatchEnd)
        {
            _sendBatchEnd = false;
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
    _loop->assertInLoopThread();
    std::shared_ptr<PGresult> res;

    if (!PQconsumeInput(_connPtr.get()))
    {
        LOG_ERROR << "Failed to consume pg input:"
                  << PQerrorMessage(_connPtr.get());
        if (_isWorking)
        {
            _isWorking = false;
            handleFatalError();
        }
        handleClosed();
        return;
    }
    if (PQisBusy(_connPtr.get()))
    {
        // need read more data from socket;
        return;
    }
    assert((!_batchCommandsForWaitingResults.empty() ||
            !_batchSqlCommands.empty()));

    while (!PQisBusy(_connPtr.get()))
    {
        res = std::shared_ptr<PGresult>(PQgetResult(_connPtr.get()),
                                        [](PGresult *p) { PQclear(p); });
        if (!res)
        {
            /*
             * No more results from this query, advance to
             * the next result
             */
            if (!PQgetNextQuery(_connPtr.get()))
            {
                return;
            }
            continue;
        }
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR ||
            type == PGRES_BATCH_ABORTED)
        {
            handleFatalError();
            continue;
        }
        if (type == PGRES_BATCH_END)
        {
            if (_batchCommandsForWaitingResults.empty() &&
                _batchSqlCommands.empty())
            {
                _isWorking = false;
                _idleCb();
                return;
            }
            continue;
        }
        if (!_batchCommandsForWaitingResults.empty())
        {
            auto &cmd = _batchCommandsForWaitingResults.front();
            if (!cmd->_preparingStatement.empty())
            {
                _preparedStatementMap[cmd->_sql] =
                    std::move(cmd->_preparingStatement);
                cmd->_preparingStatement.clear();
                continue;
            }
            auto r = makeResult(res, _sql);
            cmd->_cb(r);
            _batchCommandsForWaitingResults.pop_front();
            continue;
        }
        assert(!_batchSqlCommands.empty());
        assert(!_batchSqlCommands.front()->_preparingStatement.empty());
        auto &cmd = _batchSqlCommands.front();
        if (!cmd->_preparingStatement.empty())
        {
            _preparedStatementMap[cmd->_sql] =
                std::move(cmd->_preparingStatement);
            cmd->_preparingStatement.clear();
            continue;
        }
    }
}

void PgConnection::doAfterPreparing()
{
}

void PgConnection::handleFatalError()
{
    LOG_ERROR << PQerrorMessage(_connPtr.get());
    try
    {
        throw Failure(PQerrorMessage(_connPtr.get()));
    }
    catch (...)
    {
        auto exceptPtr = std::current_exception();
        for (auto &cmd : _batchCommandsForWaitingResults)
        {
            cmd->_exceptCb(exceptPtr);
        }
        for (auto &cmd : _batchSqlCommands)
        {
            cmd->_exceptCb(exceptPtr);
        }
        _batchCommandsForWaitingResults.clear();
        _batchSqlCommands.clear();
    }
}

void PgConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands)
{
    _loop->assertInLoopThread();
    _batchSqlCommands = std::move(sqlCommands);
    sendBatchedSql();
}
