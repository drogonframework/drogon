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
#include <drogon/utils/Utilities.h>
#include <regex>
#include <algorithm>
#include <poll.h>

using namespace drogon::orm;

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
                    outputError();
                    //FIXME exception callback;
                    return;
                }
                LOG_TRACE << "stmt ready!";
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

    _stmtPtr = std::shared_ptr<MYSQL_STMT>(mysql_stmt_init(_mysqlPtr.get()), [](MYSQL_STMT *stmt) {
        //blocking method;
        mysql_stmt_close(stmt);
    });

    if (!_stmtPtr)
    {
        LOG_ERROR << " mysql_stmt_init(), out of memory";
        //FIXME,exception callback
        return;
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

    // /* Get the parameter count from the statement */
    // auto param_count = mysql_stmt_param_count(stmt);
    // if (param_count != paraNum)
    // {
    //     //FIXME,exception callback
    //     return;
    // }
    // MYSQL_BIND *bind = new MYSQL_BIND[param_count];
    // for (int i = 0; i < param_count; i++)
    // {
    //     bind[i].buffer_type = (enum_field_types)format[i];
    //     bind[i].buffer = (char *)parameters[i];
    //     bind[i].buffer_length = length[i];
    //     bind[i].is_null = parameters[i] == NULL ? 1 : 0;
    //     bind[i].length = &length[i];
    // }
    //delete[] bind;
}

void MysqlConnection::outputError()
{
    LOG_ERROR << "mysql_stmt_prepare_start(), Error("
              << mysql_errno(_mysqlPtr.get()) << ") [" << mysql_sqlstate(_mysqlPtr.get()) << "] \""
              << mysql_error(_mysqlPtr.get()) << "\"";
}