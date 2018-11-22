//
// Created by antao on 2018/6/22.
//
#include "DbClientImpl.h"
#include "DbConnection.h"
#if USE_POSTGRESQL
#include "postgresql_impl/PgConnection.h"
#endif
#if USE_MYSQL
#include "mysql_impl/MysqlConnection.h"
#endif
#include "TransactionImpl.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/DbClient.h>
#include <sys/select.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <sstream>

using namespace drogon::orm;

DbClientImpl::DbClientImpl(const std::string &connInfo, const size_t connNum, ClientType type)
    : _connInfo(connInfo),
      _connectNum(connNum),
      _type(type)
{
    assert(connNum > 0);
    _loopThread = std::thread([=]() {
        _loopPtr = std::shared_ptr<trantor::EventLoop>(new trantor::EventLoop);
        ioLoop();
    });
}
void DbClientImpl::ioLoop()
{

    for (size_t i = 0; i < _connectNum; i++)
    {
        _connections.insert(newConnection());
    }
    _loopPtr->loop();
}

DbClientImpl::~DbClientImpl()
{
    _stop = true;
    _loopPtr->quit();
    if (_loopThread.joinable())
        _loopThread.join();
}

void DbClientImpl::execSql(const DbConnectionPtr &conn,
                           const std::string &sql,
                           size_t paraNum,
                           const std::vector<const char *> &parameters,
                           const std::vector<int> &length,
                           const std::vector<int> &format,
                           const ResultCallback &cb,
                           const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    if (!conn)
    {
        try
        {
            throw BrokenConnection("There is no connection to PG server!");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
        return;
    }
    std::weak_ptr<DbConnection> weakConn = conn;
    conn->execSql(sql, paraNum, parameters, length, format,
                  cb, exceptCallback,
                  [=]() -> void {
                      {
                          auto connPtr = weakConn.lock();
                          if (!connPtr)
                              return;
                          handleNewTask(connPtr);
                      }
                  });
}
void DbClientImpl::execSql(const std::string &sql,
                           size_t paraNum,
                           const std::vector<const char *> &parameters,
                           const std::vector<int> &length,
                           const std::vector<int> &format,
                           const QueryCallback &cb,
                           const ExceptPtrCallback &exceptCb)
{
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(cb);
    DbConnectionPtr conn;
    {
        std::lock_guard<std::mutex> guard(_connectionsMutex);

        if (_readyConnections.size() == 0)
        {
            if (_busyConnections.size() == 0)
            {
                try
                {
                    throw BrokenConnection("No connection to postgreSQL server");
                }
                catch (...)
                {
                    exceptCb(std::current_exception());
                }
                return;
            }
        }
        else
        {
            auto iter = _readyConnections.begin();
            _busyConnections.insert(*iter);
            conn = *iter;
            _readyConnections.erase(iter);
        }
    }
    if (conn)
    {
        execSql(conn, sql, paraNum, parameters, length, format, cb, exceptCb);
        return;
    }
    bool busy = false;
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        if (_sqlCmdBuffer.size() > 10000)
        {
            //too many queries in buffer;
            busy = true;
        }
    }
    if (busy)
    {
        try
        {
            throw Failure("Too many queries in buffer");
        }
        catch (...)
        {
            exceptCb(std::current_exception());
        }
        return;
    }
    //LOG_TRACE << "Push query to buffer";
    SqlCmd cmd;
    cmd._sql = sql;
    cmd._paraNum = paraNum;
    cmd._parameters = parameters;
    cmd._length = length;
    cmd._format = format;
    cmd._cb = cb;
    cmd._exceptCb = exceptCb;
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        _sqlCmdBuffer.push_back(std::move(cmd));
    }
}

std::string DbClientImpl::replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const
{
    std::string::size_type startPos = 0;
    std::string::size_type pos;
    std::stringstream ret;
    size_t phCount = 1;
    do
    {
        pos = sqlStr.find(holderStr, startPos);
        if (pos == std::string::npos)
        {
            ret << sqlStr.substr(startPos);
            return ret.str();
        }
        ret << sqlStr.substr(startPos, pos - startPos);
        ret << "$";
        ret << phCount++;
        startPos = pos + holderStr.length();
    } while (1);
}

std::shared_ptr<Transaction> DbClientImpl::newTransaction()
{
    DbConnectionPtr conn;
    {
        std::unique_lock<std::mutex> lock(_connectionsMutex);
        _transWaitNum++;
        _condConnectionReady.wait(lock, [this]() {
            return _readyConnections.size() > 0;
        });
        _transWaitNum--;
        auto iter = _readyConnections.begin();
        _busyConnections.insert(*iter);
        conn = *iter;
        _readyConnections.erase(iter);
    }
    auto trans = std::shared_ptr<TransactionImpl>(new TransactionImpl(conn, [=]() {
        if (conn->status() == ConnectStatus_Bad)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> guard(_connectionsMutex);

            if (_connections.find(conn) == _connections.end() &&
                _busyConnections.find(conn) == _busyConnections.find(conn))
            {
                //connection is broken and removed
                return;
            }
        }
        handleNewTask(conn);
    }));
    trans->doBegin();
    return trans;
}

void DbClientImpl::handleNewTask(const DbConnectionPtr &connPtr)
{
    std::lock_guard<std::mutex> guard(_connectionsMutex);
    if (_transWaitNum > 0)
    {
        //Prioritize the needs of the transaction
        _busyConnections.erase(connPtr);
        _readyConnections.insert(connPtr);
        _condConnectionReady.notify_one();
    }
    else
    {
        //Then check if there are some sql queries in the buffer
        {
            std::lock_guard<std::mutex> guard(_bufferMutex);
            if (_sqlCmdBuffer.size() > 0)
            {
                _busyConnections.insert(connPtr); //For new connections, this sentence is necessary
                auto cmd = _sqlCmdBuffer.front();
                _sqlCmdBuffer.pop_front();
                _loopPtr->queueInLoop([=]() {
                    execSql(connPtr, cmd._sql, cmd._paraNum, cmd._parameters, cmd._length, cmd._format, cmd._cb, cmd._exceptCb);
                });

                return;
            }
        }
        //Idle connection
        _busyConnections.erase(connPtr);
        _readyConnections.insert(connPtr);
    }
}

DbConnectionPtr DbClientImpl::newConnection()
{
    DbConnectionPtr connPtr;
    if (_type == ClientType::PostgreSQL)
    {
#if USE_POSTGRESQL
        connPtr = std::make_shared<PgConnection>(_loopPtr.get(), _connInfo);
#else
        return nullptr;
#endif
    }
    else if(_type == ClientType::Mysql)
    {
#if USE_MYSQL
        connPtr = std::make_shared<MysqlConnection>(_loopPtr.get(), _connInfo);
#else
        return nullptr;
#endif
    }
    else
    {
        return nullptr;
    }
    auto loopPtr = _loopPtr;
    std::weak_ptr<DbClientImpl> weakPtr = shared_from_this();
    connPtr->setCloseCallback([weakPtr, loopPtr](const DbConnectionPtr &closeConnPtr) {
        //Reconnect after 1 second
        loopPtr->runAfter(1, [weakPtr, closeConnPtr] {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
            thisPtr->_readyConnections.erase(closeConnPtr);
            thisPtr->_busyConnections.erase(closeConnPtr);
            assert(thisPtr->_connections.find(closeConnPtr) != thisPtr->_connections.end());
            thisPtr->_connections.erase(closeConnPtr);
            thisPtr->_connections.insert(thisPtr->newConnection());
        });
    });
    connPtr->setOkCallback([weakPtr](const DbConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        thisPtr->handleNewTask(okConnPtr);
    });
    //std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}
