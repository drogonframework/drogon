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
#include <memory>
#include <stdio.h>
#include <trantor/utils/Logger.h>

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
                _channel.disableWriting();
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
    _loop->assertInLoopThread();
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(!_isWorking);
    assert(!sql.empty());
    _sql = std::move(sql);
    _cb = std::move(rcb);
    _isWorking = true;
    _exceptCb = std::move(exceptCallback);
    if (paraNum == 0)
    {
        _isRreparingStatement = false;
        if (PQsendQuery(_connPtr.get(), _sql.c_str()) == 0)
        {
            LOG_ERROR << "send query error: " << PQerrorMessage(_connPtr.get());
            if (_isWorking)
            {
                _isWorking = false;
                _isRreparingStatement = false;
                handleFatalError();
                _cb = nullptr;
                _idleCb();
            }
            return;
        }
        flush();
    }
    else
    {
        auto iter = _preparedStatementMap.find(_sql);
        if (iter != _preparedStatementMap.end())
        {
            _isRreparingStatement = false;
            if (PQsendQueryPrepared(_connPtr.get(),
                                    iter->second.c_str(),
                                    paraNum,
                                    parameters.data(),
                                    length.data(),
                                    format.data(),
                                    0) == 0)
            {
                LOG_ERROR << "send query error: "
                          << PQerrorMessage(_connPtr.get());
                if (_isWorking)
                {
                    _isWorking = false;
                    _isRreparingStatement = false;
                    handleFatalError();
                    _cb = nullptr;
                    _idleCb();
                }
                return;
            }
        }
        else
        {
            _isRreparingStatement = true;
            _statementName = utils::getUuid();
            if (PQsendPrepare(_connPtr.get(),
                              _statementName.c_str(),
                              _sql.c_str(),
                              paraNum,
                              NULL) == 0)
            {
                LOG_ERROR << "send query error: "
                          << PQerrorMessage(_connPtr.get());
                if (_isWorking)
                {
                    _isWorking = false;
                    handleFatalError();
                    _cb = nullptr;
                    _idleCb();
                }
                return;
            }
            _paraNum = paraNum;
            _parameters = std::move(parameters);
            _length = std::move(length);
            _format = std::move(format);
        }
        flush();
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
            _cb = nullptr;
        }
        handleClosed();
        return;
    }
    if (PQisBusy(_connPtr.get()))
    {
        // need read more data from socket;
        return;
    }
    while ((res = std::shared_ptr<PGresult>(PQgetResult(_connPtr.get()),
                                            [](PGresult *p) { PQclear(p); })))
    {
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR)
        {
            LOG_WARN << PQerrorMessage(_connPtr.get());
            if (_isWorking)
            {
                handleFatalError();
                _cb = nullptr;
            }
        }
        else
        {
            if (_isWorking)
            {
                if (!_isRreparingStatement)
                {
                    auto r = makeResult(res, _sql);
                    _cb(r);
                    _cb = nullptr;
                    _exceptCb = nullptr;
                }
            }
        }
    }
    if (_isWorking)
    {
        if (_isRreparingStatement && _cb)
        {
            doAfterPreparing();
        }
        else
        {
            _isWorking = false;
            _isRreparingStatement = false;
            _idleCb();
        }
    }
}

void PgConnection::doAfterPreparing()
{
    _isRreparingStatement = false;
    _preparedStatementMap[_sql] = _statementName;
    if (PQsendQueryPrepared(_connPtr.get(),
                            _statementName.c_str(),
                            _paraNum,
                            _parameters.data(),
                            _length.data(),
                            _format.data(),
                            0) == 0)
    {
        LOG_ERROR << "send query error: " << PQerrorMessage(_connPtr.get());
        if (_isWorking)
        {
            _isWorking = false;
            handleFatalError();
            _cb = nullptr;
            _idleCb();
        }
        return;
    }
    flush();
}

void PgConnection::handleFatalError()
{
    try
    {
        throw Failure(PQerrorMessage(_connPtr.get()));
    }
    catch (...)
    {
        auto exceptPtr = std::current_exception();
        _exceptCb(exceptPtr);
        _exceptCb = nullptr;
    }
}

void PgConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&)
{
    assert(false);
}