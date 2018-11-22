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
    int status = mysql_real_connect_start(&ret, _mysqlPtr.get(), "127.0.0.1", "root", "", "test", 3306, NULL, 0);
    
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
    LOG_TRACE << "channel index:" << _channelPtr->index();
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
    LOG_TRACE << "channel index:" << _channelPtr->index();
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