/**
 *
 *  MysqlConnection.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "MysqlConnection.h"
#include "MysqlResultImpl.h"
#include "MysqlStmtResultImpl.h"
#include <drogon/utils/Utilities.h>
#include <regex>
#include <algorithm>
#include <poll.h>

using namespace drogon::orm;
namespace drogon
{
namespace orm
{

Result makeResult(const std::shared_ptr<MYSQL_RES> &r = std::shared_ptr<MYSQL_RES>(nullptr),
                  const std::string &query = "",
                  Result::size_type affectedRows = 0)
{
    return Result(std::shared_ptr<MysqlResultImpl>(new MysqlResultImpl(r, query, affectedRows)));
}

Result makeResult(const std::shared_ptr<MYSQL_STMT> &r = std::shared_ptr<MYSQL_STMT>(nullptr),
                  const std::string &query = "")
{
    return Result(std::shared_ptr<MysqlStmtResultImpl>(new MysqlStmtResultImpl(r, query)));
}


} // namespace orm
} // namespace drogon

MysqlConnection::MysqlConnection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop)
{
    _mysqlPtr = std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) {
        mysql_close(p);
    });
    mysql_init(_mysqlPtr.get());
    mysql_options(_mysqlPtr.get(), MYSQL_OPT_NONBLOCK, 0);

    //Get the key and value
    std::regex r(" *= *");
    auto tmpStr = std::regex_replace(connInfo, r, "=");
    std::string host, user, passwd, dbname, port;
    auto keyValues = splitString(tmpStr, " ");
    for (auto &kvs : keyValues)
    {
        auto kv = splitString(kvs, "=");
        assert(kv.size() == 2);
        auto key = kv[0];
        auto value = kv[1];
        if (value[0] == '\'' && value[value.length() - 1] == '\'')
        {
            value = value.substr(1, value.length() - 2);
        }
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        //LOG_TRACE << key << "=" << value;
        if (key == "host")
        {
            host = value;
        }
        else if (key == "user")
        {
            user = value;
        }
        else if (key == "dbname")
        {
            dbname = value;
        }
        else if (key == "port")
        {
            port = value;
        }
        else if (key == "password")
        {
            passwd = value;
        }
    }
    _loop->queueInLoop([=]() {
        MYSQL *ret;
        _status = ConnectStatus_Connecting;
        _waitStatus = mysql_real_connect_start(&ret,
                                               _mysqlPtr.get(),
                                               host.empty() ? NULL : host.c_str(),
                                               user.empty() ? NULL : user.c_str(),
                                               passwd.empty() ? NULL : passwd.c_str(),
                                               dbname.empty() ? NULL : dbname.c_str(),
                                               port.empty() ? 3306 : atol(port.c_str()),
                                               NULL,
                                               0);

        auto fd = mysql_get_socket(_mysqlPtr.get());
        _channelPtr = std::unique_ptr<trantor::Channel>(new trantor::Channel(loop, fd));
        _channelPtr->setCloseCallback([=]() {
            perror("sock close");
            handleClosed();
        });
        _channelPtr->setEventCallback([=]() {
            handleEvent();
        });
        setChannel();
    });
}

void MysqlConnection::setChannel()
{
    _channelPtr->disableAll();
    if ((_waitStatus & MYSQL_WAIT_READ) || (_waitStatus & MYSQL_WAIT_EXCEPT))
    {
        _channelPtr->enableReading();
    }
    if (_waitStatus & MYSQL_WAIT_WRITE)
    {
        _channelPtr->enableWriting();
    }
    //(status & MYSQL_WAIT_EXCEPT) ///FIXME
    if (_waitStatus & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value(_mysqlPtr.get());
        auto thisPtr = shared_from_this();
        _loop->runAfter(timeout, [thisPtr]() {
            thisPtr->handleTimeout();
        });
    }
}

void MysqlConnection::handleClosed()
{
    _status = ConnectStatus_Bad;
    _loop->assertInLoopThread();
    _channelPtr->disableAll();
    _channelPtr->remove();
    assert(_closeCb);
    auto thisPtr = shared_from_this();
    _closeCb(thisPtr);
}
void MysqlConnection::handleTimeout()
{
    LOG_TRACE << "channel index:" << _channelPtr->index();
    int status = 0;
    status |= MYSQL_WAIT_TIMEOUT;
    MYSQL *ret;
    if (_status == ConnectStatus_Connecting)
    {
        _waitStatus = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (_waitStatus == 0)
        {
            if (!ret)
            {
                handleClosed();
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            //I don't think the programe can run to here.
            _status = ConnectStatus_Ok;
            if (_okCb)
            {
                auto thisPtr = shared_from_this();
                _okCb(thisPtr);
            }
        }
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
    }
}
void MysqlConnection::handleEvent()
{
    int status = 0;
    auto revents = _channelPtr->revents();
    if (revents & POLLIN)
        status |= MYSQL_WAIT_READ;
    if (revents & POLLOUT)
        status |= MYSQL_WAIT_WRITE;
    if (revents & POLLPRI)
        status |= MYSQL_WAIT_EXCEPT;
    status = (status & _waitStatus);
    MYSQL *ret;
    if (_status == ConnectStatus_Connecting)
    {
        _waitStatus = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (_waitStatus == 0)
        {
            if (!ret)
            {
                handleClosed();
                perror("");
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            _status = ConnectStatus_Ok;
            if (_okCb)
            {
                auto thisPtr = shared_from_this();
                _okCb(thisPtr);
            }
        }
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
        switch (_execStatus)
        {
        case ExecStatus_StmtPrepare:
        {
            int err;
            _waitStatus = mysql_stmt_prepare_cont(&err, _stmtPtr.get(), status);
            if (_waitStatus == 0)
            {
                _execStatus = ExecStatus_None;
                if (err)
                {
                    outputStmtError();
                    //FIXME exception callback;
                    return;
                }
                LOG_TRACE << "stmt ready!";
                if (mysql_stmt_bind_param(_stmtPtr.get(), _binds.data()))
                {
                    outputStmtError();
                    return;
                }
                _waitStatus = mysql_stmt_execute_start(&err, _stmtPtr.get());
                LOG_TRACE << "mysql_stmt_execute_start:" << _waitStatus;
                _execStatus = ExecStatus_StmtExec;
                if (_waitStatus == 0)
                {
                    _execStatus = ExecStatus_None;
                    if (err)
                    {
                        outputStmtError();
                        return;
                    }
                    
                    _waitStatus = mysql_stmt_store_result_start(&err, _stmtPtr.get());
                    _execStatus = ExecStatus_StmtStoreResult;
                    if (_waitStatus == 0)
                    {
                        _execStatus = ExecStatus_None;
                        if (err)
                        {
                            outputStmtError();
                            return;
                        }
                        getStmtResult();
                    }
                }
            }
            setChannel();
            break;
        }
        case ExecStatus_RealQuery:
        {
            int err;
            _waitStatus = mysql_real_query_cont(&err, _mysqlPtr.get(), status);
            LOG_TRACE << "real_query:" << _waitStatus;
            if (_waitStatus == 0)
            {
                if (err)
                {
                    _execStatus = ExecStatus_None;
                    outputError();
                    //FIXME exception callback;
                    return;
                }
                _execStatus = ExecStatus_StoreResult;
                MYSQL_RES *ret;
                _waitStatus = mysql_store_result_start(&ret, _mysqlPtr.get());
                LOG_TRACE << "store_result_start:" << _waitStatus;
                if (_waitStatus == 0)
                {
                    _execStatus = ExecStatus_None;
                    if (err)
                    {
                        outputError();
                        //FIXME exception callback;
                        return;
                    }
                    getResult(ret);
                }
            }
            setChannel();
            break;
        }
        case ExecStatus_StoreResult:
        {
            MYSQL_RES *ret;
            _waitStatus = mysql_store_result_cont(&ret, _mysqlPtr.get(), status);
            LOG_TRACE << "store_result:" << _waitStatus;
            if (_waitStatus == 0)
            {
                if (!ret)
                {
                    _execStatus = ExecStatus_None;
                    outputError();
                    //FIXME exception callback;
                    return;
                }
                //make result!
                getResult(ret);
            }
            setChannel();
            break;
        }
        case ExecStatus_StmtExec:
        {
            int err;
            _waitStatus = mysql_stmt_execute_cont(&err, _stmtPtr.get(), status);
            LOG_TRACE << "mysql_stmt_execute_cont:" << _waitStatus;
            if (_waitStatus == 0)
            {
                _execStatus = ExecStatus_None;
                if (err)
                {
                    outputStmtError();
                    return;
                }
                _waitStatus = mysql_stmt_store_result_start(&err, _stmtPtr.get());
                LOG_TRACE << "mysql_stmt_store_result_start:" << _waitStatus;
                _execStatus = ExecStatus_StmtStoreResult;
                if (_waitStatus == 0)
                {
                    _execStatus = ExecStatus_None;
                    if (err)
                    {
                        outputStmtError();
                        return;
                    }
                    getStmtResult();
                }
            }
            setChannel();
            break;
        }
        case ExecStatus_StmtStoreResult:
        {
            int err;
            _waitStatus = mysql_stmt_store_result_cont(&err, _stmtPtr.get(), status);
            LOG_TRACE << "mysql_stmt_store_result_cont:" << _waitStatus;
            if (_waitStatus == 0)
            {
                _execStatus = ExecStatus_None;
                if (err)
                {
                    outputStmtError();
                    return;
                }
                getStmtResult();
            }
            setChannel();
            break;
        }
        default:
            return;
        }
    }
}

void MysqlConnection::execSql(const std::string &sql,
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
    if (paraNum == 0)
    {
        _loop->runInLoop([=]() {
            int err;
            //int mysql_real_query_start(int *ret, MYSQL *mysql, const char *q, unsigned long length)
            _waitStatus = mysql_real_query_start(&err, _mysqlPtr.get(), sql.c_str(), sql.length());
            LOG_TRACE << "real_query:" << _waitStatus;
            _execStatus = ExecStatus_RealQuery;
            if (_waitStatus == 0)
            {
                if (err)
                {
                    outputError();
                    return;
                }

                MYSQL_RES *ret;
                _waitStatus = mysql_store_result_start(&ret, _mysqlPtr.get());
                LOG_TRACE << "store_result:" << _waitStatus;
                _execStatus = ExecStatus_StoreResult;
                if (_waitStatus == 0)
                {
                    _execStatus = ExecStatus_None;
                    if (!ret)
                    {
                        outputError();
                        return;
                    }
                    getResult(ret);
                }
            }
            setChannel();
        });
        return;
    }

    _stmtPtr = std::shared_ptr<MYSQL_STMT>(mysql_stmt_init(_mysqlPtr.get()), [](MYSQL_STMT *stmt) {
        //Is it a blocking method?
        mysql_stmt_close(stmt);
    });
    if (!_stmtPtr)
    {
        outputError();
        return;
    }
    
    my_bool flag = 1;
    mysql_stmt_attr_set(_stmtPtr.get(), STMT_ATTR_UPDATE_MAX_LENGTH, &flag);

    _binds.resize(paraNum);
    _lengths.resize(paraNum);
    _isNulls.resize(paraNum);
    memset(_binds.data(), 0, sizeof(MYSQL_BIND) * paraNum);
    for (size_t i = 0; i < paraNum; i++)
    {
        _binds[i].buffer = (void *)parameters[i];
        _binds[i].buffer_type = (enum_field_types)format[i];
        _binds[i].buffer_length = length[i];
        _binds[i].length = (length[i] == 0 ? 0 : (_lengths[i] = length[i], &_lengths[i]));
        _binds[i].is_null = (parameters[i] != NULL ? 0 : (_isNulls[i] = true, &_isNulls[i]));
    }
    _loop->runInLoop([=]() {
        int err;
        _waitStatus = mysql_stmt_prepare_start(&err, _stmtPtr.get(), sql.c_str(), sql.length());
        if (_waitStatus == 0)
        {
            if (err)
            {
                outputError();
                return;
            }
        }

        _execStatus = ExecStatus_StmtPrepare;
        setChannel();
    });
}

void MysqlConnection::outputError()
{
    _channelPtr->disableAll();
    LOG_ERROR << "Error("
              << mysql_errno(_mysqlPtr.get()) << ") [" << mysql_sqlstate(_mysqlPtr.get()) << "] \""
              << mysql_error(_mysqlPtr.get()) << "\"";
    if (_isWorking)
    {

        try
        {
            //FIXME exception type
            throw SqlError(mysql_error(_mysqlPtr.get()),
                           _sql);
        }
        catch (...)
        {
            _exceptCb(std::current_exception());
            _exceptCb = decltype(_exceptCb)();
        }

        _cb = decltype(_cb)();
        _isWorking = false;
        if (_idleCb)
        {
            _idleCb();
            _idleCb = decltype(_idleCb)();
        }
    }
}

void MysqlConnection::outputStmtError()
{
    _channelPtr->disableAll();
    LOG_ERROR << "Error("
              << mysql_stmt_errno(_stmtPtr.get()) << ") [" << mysql_stmt_sqlstate(_stmtPtr.get()) << "] \""
              << mysql_stmt_error(_stmtPtr.get()) << "\"";
    if (_isWorking)
    {
        try
        {
            //FIXME exception type
            throw SqlError(mysql_stmt_error(_stmtPtr.get()),
                           _sql);
        }
        catch (...)
        {
            _exceptCb(std::current_exception());
            _exceptCb = decltype(_exceptCb)();
        }

        _cb = decltype(_cb)();
        _isWorking = false;
        if (_idleCb)
        {
            _idleCb();
            _idleCb = decltype(_idleCb)();
        }
    }
}

void MysqlConnection::getResult(MYSQL_RES *res)
{
    auto resultPtr = std::shared_ptr<MYSQL_RES>(res, [](MYSQL_RES *r) {
        mysql_free_result(r);
    });
    auto Result = makeResult(resultPtr, _sql, mysql_affected_rows(_mysqlPtr.get()));
    if (_isWorking)
    {
        _cb(Result);
        _cb = decltype(_cb)();
        _exceptCb = decltype(_exceptCb)();
        _isWorking = false;
        _idleCb();
        _idleCb = decltype(_idleCb)();
    }
}

void MysqlConnection::getStmtResult()
{
    LOG_TRACE << "Got " << mysql_stmt_num_rows(_stmtPtr.get()) << " rows";
    auto Result = makeResult(_stmtPtr, _sql);
    if (_isWorking)
    {
        _cb(Result);
        _cb = decltype(_cb)();
        _exceptCb = decltype(_exceptCb)();
        _isWorking = false;
        _idleCb();
        _idleCb = decltype(_idleCb)();
    }
}