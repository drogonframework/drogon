#include "PgConnection.h"
#include "PostgreSQLResultImpl.h"
#include <trantor/utils/Logger.h>
#include <drogon/orm/Exception.h>
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
    : _connPtr(std::shared_ptr<PGconn>(PQconnectStart(connInfo.c_str()), [](PGconn *conn) {
          PQfinish(conn);
      })),
      _loop(loop), _channel(_loop, PQsocket(_connPtr.get()))
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
int PgConnection::sock()
{
    return PQsocket(_connPtr.get());
}
void PgConnection::handleClosed()
{
    _status = ConnectStatus_Bad;
    _loop->assertInLoopThread();
    _channel.disableAll();
    _channel.remove();
    assert(_closeCb);
    auto thisPtr = shared_from_this();
    _closeCb(thisPtr);
}
void PgConnection::pgPoll()
{
    _loop->assertInLoopThread();
    auto connStatus = PQconnectPoll(_connPtr.get());

    switch (connStatus)
    {
    case PGRES_POLLING_FAILED:
        LOG_ERROR << "!!!Pg connection failed: " << PQerrorMessage(_connPtr.get());
        break;
    case PGRES_POLLING_WRITING:
        _channel.enableWriting();
        _channel.disableReading();
        break;
    case PGRES_POLLING_READING:
        _channel.enableReading();
        _channel.disableWriting();
        break;

    case PGRES_POLLING_OK:
        if (_status != ConnectStatus_Ok)
        {
            _status = ConnectStatus_Ok;
            assert(_okCb);
            _okCb(shared_from_this());
        }
        _channel.enableReading();
        _channel.disableWriting();
        break;
    case PGRES_POLLING_ACTIVE:
        //unused!
        break;
    default:
        break;
    }
}
void PgConnection::execSql(const std::string &sql,
                           size_t paraNum,
                           const std::vector<const char *> &parameters,
                           const std::vector<int> &length,
                           const std::vector<int> &format,
                           const ResultCallback &rcb,
                           const std::function<void(const std::exception_ptr &)> &exceptCallback,
                           const std::function<void()> &idleCb)
{
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(idleCb);
    assert(!_isWorking);
    assert(!sql.empty());
    _sql = sql;
    _cb = rcb;
    _idleCb = idleCb;
    _isWorking = true;
    _exceptCb = exceptCallback;
    //_channel.enableWriting();
    LOG_TRACE << sql;
    // for (size_t i = 0; i < paraNum;i++)
    // {
    //     LOG_TRACE << "parameter[" << i << "]=" << ntohl(*(int *)parameters[i]);
    // }
    if (PQsendQueryParams(
            _connPtr.get(),
            sql.c_str(),
            paraNum,
            NULL,
            parameters.data(),
            length.data(),
            format.data(),
            0) == 0)
    {
        LOG_ERROR << "send query error: " << PQerrorMessage(_connPtr.get());
        // connection broken! will be handled in handleRead()
        // _loop->queueInLoop([=]() {
        //     try
        //     {
        //         throw InternalError(PQerrorMessage(_connPtr.get()));
        //     }
        //     catch (...)
        //     {
        //         _isWorking = false;
        //         _exceptCb(std::current_exception());
        //         _exceptCb = decltype(_exceptCb)();
        //         if (_idleCb)
        //         {
        //             _idleCb();
        //             _idleCb = decltype(_idleCb)();
        //         }
        //     }
        // });
        // return;
    }
    auto thisPtr = shared_from_this();
    _loop->runInLoop([=]() {
        thisPtr->pgPoll();
    });
}
void PgConnection::handleRead()
{

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
            if (_idleCb)
            {
                _idleCb();
                _idleCb = decltype(_idleCb)();
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

    _channel.disableWriting();
    // got query results?
    while ((res = std::shared_ptr<PGresult>(PQgetResult(_connPtr.get()), [](PGresult *p) {
                PQclear(p);
            })))
    {
        auto type = PQresultStatus(res.get());
        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR)
        {
            LOG_ERROR << "Result error: %s" << PQerrorMessage(_connPtr.get());
            if (_isWorking)
            {
                {
                    try
                    {
                        //FIXME exception type
                        throw SqlError(PQerrorMessage(_connPtr.get()),
                                       _sql);
                    }
                    catch (...)
                    {
                        _exceptCb(std::current_exception());
                        _exceptCb = decltype(_exceptCb)();
                    }
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
        if (_idleCb)
        {
            _idleCb();
            _idleCb = decltype(_idleCb)();
        }
    }
}
