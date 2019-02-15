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
#include <trantor/utils/Logger.h>
#include <drogon/orm/Exception.h>
#include <memory>
#include <stdio.h>

using namespace drogon::orm;

namespace drogon
{
namespace orm
{

Result makeResult(const std::shared_ptr<PGresult> &r = std::shared_ptr<PGresult>(nullptr), const std::string &query = "")
{
    return Result(std::shared_ptr<PostgreSQLResultImpl>(new PostgreSQLResultImpl(r, query)));
}

} // namespace orm
} // namespace drogon

PgConnection::PgConnection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop),
      _connPtr(std::shared_ptr<PGconn>(PQconnectStart(connInfo.c_str()), [](PGconn *conn) {
          PQfinish(conn);
      })),
      _channel(loop, PQsocket(_connPtr.get()))
{
    PQsetnonblocking(_connPtr.get(), 1);
    //assert(PQisnonblocking(_connPtr.get()));
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
        if (_status != ConnectStatus_Ok)
        {
            pgPoll();
        }
        else
        {
            PQconsumeInput(_connPtr.get());
        }
    });
    _channel.setCloseCallback([=]() {
        perror("sock close");
        handleClosed();
    });
    _channel.setErrorCallback([=]() {
        perror("sock err");
        handleClosed();
    });
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
        LOG_ERROR << "!!!Pg connection failed: " << PQerrorMessage(_connPtr.get());
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
        //unused!
        break;
    default:
        break;
    }
}

void PgConnection::execSql(std::string &&sql,
                           size_t paraNum,
                           std::vector<const char *> &&parameters,
                           std::vector<int> &&length,
                           std::vector<int> &&format,
                           ResultCallback &&rcb,
                           std::function<void(const std::exception_ptr &)> &&exceptCallback,
                           std::function<void()> &&idleCb)
{
    LOG_TRACE << sql;
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(idleCb);
    assert(!_isWorking);
    assert(!sql.empty());
    _sql = std::move(sql);
    _cb = std::move(rcb);
    _idleCbPtr = std::make_shared<std::function<void()>>(std::move(idleCb));
    _isWorking = true;
    _exceptCb = std::move(exceptCallback);
    auto thisPtr = shared_from_this();
    if (!_loop->isInLoopThread())
    {
        _loop->queueInLoop([thisPtr, paraNum = std::move(paraNum), parameters = std::move(parameters), length = std::move(length), format = std::move(format)]() {
            if (PQsendQueryParams(
                    thisPtr->_connPtr.get(),
                    thisPtr->_sql.c_str(),
                    paraNum,
                    NULL,
                    parameters.data(),
                    length.data(),
                    format.data(),
                    0) == 0)
            {
                LOG_ERROR << "send query error: " << PQerrorMessage(thisPtr->_connPtr.get());
                if (thisPtr->_isWorking)
                {
                    thisPtr->_isWorking = false;
                    try
                    {
                        throw Failure(PQerrorMessage(thisPtr->_connPtr.get()));
                    }
                    catch (...)
                    {
                        auto exceptPtr = std::current_exception();
                        thisPtr->_exceptCb(exceptPtr);
                        thisPtr->_exceptCb = decltype(_exceptCb)();
                    }
                    thisPtr->_cb = decltype(_cb)();
                    if (thisPtr->_idleCbPtr)
                    {
                        auto idle = std::move(thisPtr->_idleCbPtr);
                        thisPtr->_idleCbPtr.reset();
                        (*idle)();
                    }
                }
                return;
            }
            thisPtr->pgPoll();
        });
    }
    else
    {
        if (PQsendQueryParams(
                _connPtr.get(),
                _sql.c_str(),
                paraNum,
                NULL,
                parameters.data(),
                length.data(),
                format.data(),
                0) == 0)
        {
            LOG_ERROR << "send query error: " << PQerrorMessage(thisPtr->_connPtr.get());
            if (_isWorking)
            {
                _isWorking = false;
                try
                {
                    throw Failure(PQerrorMessage(_connPtr.get()));
                }
                catch (...)
                {
                    auto exceptPtr = std::current_exception();
                    _exceptCb(exceptPtr);
                    _exceptCb = decltype(_exceptCb)();
                }
                _cb = decltype(_cb)();
                if (_idleCbPtr)
                {
                    auto idle = std::move(_idleCbPtr);
                    _idleCbPtr.reset();
                    (*idle)();
                }
            }
            return;
        }
        thisPtr->pgPoll();
    }
}

void PgConnection::handleRead()
{
    _loop->assertInLoopThread();
    std::shared_ptr<PGresult> res;

    if (!PQconsumeInput(_connPtr.get()))
    {
        LOG_ERROR << "Failed to consume pg input:" << PQerrorMessage(_connPtr.get());
        if (_isWorking)
        {
            _isWorking = false;
            try
            {
                throw BrokenConnection(PQerrorMessage(_connPtr.get()));
            }
            catch (...)
            {
                auto exceptPtr = std::current_exception();
                _exceptCb(exceptPtr);
                _exceptCb = decltype(_exceptCb)();
            }
            _cb = decltype(_cb)();
            if (_idleCbPtr)
            {
                auto idle = std::move(_idleCbPtr);
                _idleCbPtr.reset();
                (*idle)();
            }
        }
        handleClosed();
        return;
    }
    if (PQisBusy(_connPtr.get()))
    {
        //need read more data from socket;
        return;
    }
    if (_channel.isWriting())
        _channel.disableWriting();
    while ((res = std::shared_ptr<PGresult>(PQgetResult(_connPtr.get()), [](PGresult *p) {
                PQclear(p);
            })))
    {
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR)
        {
            LOG_WARN << PQerrorMessage(_connPtr.get());
            if (_isWorking)
            {
                try
                {
                    //TODO: exception type
                    throw SqlError(PQerrorMessage(_connPtr.get()), _sql);
                }
                catch (...)
                {
                    _exceptCb(std::current_exception());
                    _exceptCb = decltype(_exceptCb)();
                }
                _cb = decltype(_cb)();
            }
        }
        else
        {
            if (_isWorking)
            {
                auto r = makeResult(res, _sql);
                _cb(r);
                _cb = decltype(_cb)();
                _exceptCb = decltype(_exceptCb)();
            }
        }
    }
    if (_isWorking)
    {
        _isWorking = false;
        if (_idleCbPtr)
        {
            auto idle = std::move(_idleCbPtr);
            _idleCbPtr.reset();
            (*idle)();
        }
    }
}
