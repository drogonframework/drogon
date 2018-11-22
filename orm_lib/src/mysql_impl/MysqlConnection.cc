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

using namespace drogon::orm;

MysqlConnection::MysqlConnection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop)
{
    _mysqlPtr = std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) {
        mysql_close(p);
    });
    mysql_init(_mysqlPtr.get());
    mysql_options(_mysqlPtr.get(), MYSQL_OPT_NONBLOCK, 0);
    MYSQL *ret;
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

    int status = mysql_real_connect_start(&ret,
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

    _channelPtr->setReadCallback([=]() {
        handleRead();
    });
    _channelPtr->setWriteCallback([=]() {
        handleWrite();
    });
    _channelPtr->setCloseCallback([=]() {
        perror("sock close");
        handleClosed();
    });
    _channelPtr->setErrorCallback([=]() {
        perror("sock err");
        handleClosed();
    });
    // LOG_TRACE << "channel index:" << _channelPtr->index();
    // LOG_TRACE << "channel " << this << " fd:" << _channelPtr->fd();
    _channelPtr->enableReading();
    setChannel(status);
}

void MysqlConnection::setChannel(int status)
{
    _channelPtr->disableReading();
    _channelPtr->disableWriting();
    if (status & MYSQL_WAIT_READ)
    {
        _channelPtr->enableReading();
    }
    if (status & MYSQL_WAIT_WRITE)
    {
        _channelPtr->enableWriting();
    }
    //(status & MYSQL_WAIT_EXCEPT) ///FIXME
    if (status & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value(_mysqlPtr.get());
        auto thisPtr = shared_from_this();
        _loop->runAfter(timeout, [thisPtr]() {
            thisPtr->handleTimeout();
        });
    }
}
void MysqlConnection::handleRead()
{
    int status = 0;
    status |= MYSQL_WAIT_READ;
    MYSQL *ret;
    if (_status != ConnectStatus_Ok)
    {
        status = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (status == 0)
        {
            if (!ret)
            {
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }

            _status = ConnectStatus_Ok;
            LOG_TRACE << "connected!!!";
            return;
        }
        else
        {
            setChannel(status);
        }
    }
}
void MysqlConnection::handleWrite()
{
    LOG_TRACE << "channel index:" << _channelPtr->index();
    int status = 0;
    status |= MYSQL_WAIT_WRITE;
    MYSQL *ret;
    if (_status != ConnectStatus_Ok)
    {
        status = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (status == 0)
        {
            if (!ret)
            {
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            _status = ConnectStatus_Ok;
            LOG_TRACE << "connected!!!";
            return;
        }
        else
        {
            setChannel(status);
        }
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
    if (_status != ConnectStatus_Ok)
    {
        status = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (status == 0)
        {
            if (!ret)
            {
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            _status = ConnectStatus_Ok;
            LOG_TRACE << "connected!!!";
            return;
        }
        else
        {
            setChannel(status);
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
    
}