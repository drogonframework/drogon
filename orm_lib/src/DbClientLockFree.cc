/**
 *
 *  DbClientLockFree.cc
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

#include "DbClientLockFree.h"
#include "DbConnection.h"
#include "TransactionImpl.h"
#if USE_POSTGRESQL
#include "postgresql_impl/PgConnection.h"
#endif
#if USE_MYSQL
#include "mysql_impl/MysqlConnection.h"
#endif
#include "TransactionImpl.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <sys/select.h>
#include <thread>
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

using namespace drogon::orm;

DbClientLockFree::DbClientLockFree(const std::string &connInfo,
                                   trantor::EventLoop *loop,
                                   ClientType type,
                                   size_t connectionNumberPerLoop)
    : _connInfo(connInfo), _loop(loop), _connectionNum(connectionNumberPerLoop)
{
    _type = type;
    LOG_TRACE << "type=" << (int)type;
    if (type == ClientType::PostgreSQL)
    {
        _loop->queueInLoop([this]() {
            for (size_t i = 0; i < _connectionNum; i++)
                _connectionHolders.push_back(newConnection());
        });
    }
    else if (type == ClientType::Mysql)
    {
        for (size_t i = 0; i < _connectionNum; i++)
            _loop->runAfter(0.1 * (i + 1), [this]() {
                _connectionHolders.push_back(newConnection());
            });
    }
    else
    {
        LOG_ERROR << "No supported database type:" << (int)type;
    }
}

DbClientLockFree::~DbClientLockFree() noexcept
{
    for (auto &conn : _connections)
    {
        conn->disconnect();
    }
}

void DbClientLockFree::execSql(
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    _loop->assertInLoopThread();
    if (_connections.empty())
    {
        try
        {
            throw BrokenConnection("No connection to database server");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
        return;
    }
    else
    {
        for (auto &conn : _connections)
        {
            if (!conn->isWorking() &&
                (_transSet.empty() || _transSet.find(conn) == _transSet.end()))
            {
                conn->execSql(
                    std::move(sql),
                    paraNum,
                    std::move(parameters),
                    std::move(length),
                    std::move(format),
                    [callback = std::move(rcb), this](const Result &result) {
                        if (_sqlCmdBuffer.empty())
                        {
                            callback(result);
                        }
                        else
                        {
                            _loop->queueInLoop(
                                [callbackInLoop = std::move(callback),
                                 resultInLoop = std::move(result)]() {
                                    callbackInLoop(resultInLoop);
                                });
                        }
                    },
                    std::move(exceptCallback));
                return;
            }
        }
    }

    if (_sqlCmdBuffer.size() > 20000)
    {
        // too many queries in buffer;
        try
        {
            throw Failure("Too many queries in buffer");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
        return;
    }

    // LOG_TRACE << "Push query to buffer";
    _sqlCmdBuffer.emplace_back([sql = std::move(sql),
                                paraNum,
                                parameters = std::move(parameters),
                                length = std::move(length),
                                format = std::move(format),
                                rcb = std::move(rcb),
                                exceptCallback = std::move(exceptCallback),
                                this](const DbConnectionPtr &conn) mutable {
        conn->execSql(
            std::move(sql),
            paraNum,
            std::move(parameters),
            std::move(length),
            std::move(format),
            [callback = std::move(rcb), this](const Result &result) {
                if (_sqlCmdBuffer.empty())
                {
                    callback(result);
                }
                else
                {
                    _loop->queueInLoop([callbackInLoop = std::move(callback),
                                        resultInLoop = std::move(result)]() {
                        callbackInLoop(resultInLoop);
                    });
                }
            },
            std::move(exceptCallback));
    });
}

std::shared_ptr<Transaction> DbClientLockFree::newTransaction(
    const std::function<void(bool)> &commitCallback)
{
    // Don't support transaction;
    assert(0);
    return nullptr;
}

void DbClientLockFree::newTransactionAsync(
    const std::function<void(const std::shared_ptr<Transaction> &)> &callback)
{
    _loop->assertInLoopThread();
    for (auto &conn : _connections)
    {
        if (!conn->isWorking() && _transSet.find(conn) == _transSet.end())
        {
            makeTrans(conn,
                      std::function<void(const std::shared_ptr<Transaction> &)>(
                          callback));
            return;
        }
    }
    _transCallbacks.push(callback);
}

void DbClientLockFree::makeTrans(
    const DbConnectionPtr &conn,
    std::function<void(const std::shared_ptr<Transaction> &)> &&callback)
{
    std::weak_ptr<DbClientLockFree> weakThis = shared_from_this();
    auto trans = std::shared_ptr<TransactionImpl>(new TransactionImpl(
        _type, conn, std::function<void(bool)>(), [weakThis, conn]() {
            auto thisPtr = weakThis.lock();
            if (!thisPtr)
                return;

            if (conn->status() == ConnectStatus_Bad)
            {
                return;
            }
            if (!thisPtr->_transCallbacks.empty())
            {
                auto callback = std::move(thisPtr->_transCallbacks.front());
                thisPtr->_transCallbacks.pop();
                thisPtr->makeTrans(conn, std::move(callback));
                return;
            }

            for (auto &connPtr : thisPtr->_connections)
            {
                if (connPtr == conn)
                {
                    conn->loop()->queueInLoop([weakThis, conn]() {
                        auto thisPtr = weakThis.lock();
                        if (!thisPtr)
                            return;
                        std::weak_ptr<DbConnection> weakConn = conn;
                        conn->setIdleCallback([weakThis, weakConn]() {
                            auto thisPtr = weakThis.lock();
                            if (!thisPtr)
                                return;
                            auto connPtr = weakConn.lock();
                            if (!connPtr)
                                return;
                            thisPtr->handleNewTask(connPtr);
                        });
                        thisPtr->_transSet.erase(conn);
                        thisPtr->handleNewTask(conn);
                    });
                    break;
                }
            }
        }));
    _transSet.insert(conn);
    trans->doBegin();
    conn->loop()->queueInLoop(
        [callback = std::move(callback), trans] { callback(trans); });
}

void DbClientLockFree::handleNewTask(const DbConnectionPtr &conn)
{
    assert(conn);
    assert(!conn->isWorking());

    if (!_transCallbacks.empty())
    {
        auto callback = std::move(_transCallbacks.front());
        _transCallbacks.pop();
        makeTrans(conn, std::move(callback));
        return;
    }

    if (!_sqlCmdBuffer.empty())
    {
        _sqlCmdBuffer.front()(conn);
        _sqlCmdBuffer.pop_front();
        return;
    }
}

DbConnectionPtr DbClientLockFree::newConnection()
{
    DbConnectionPtr connPtr;
    if (_type == ClientType::PostgreSQL)
    {
#if USE_POSTGRESQL
        connPtr = std::make_shared<PgConnection>(_loop, _connInfo);
#else
        return nullptr;
#endif
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        connPtr = std::make_shared<MysqlConnection>(_loop, _connInfo);
#else
        return nullptr;
#endif
    }
    else
    {
        return nullptr;
    }

    std::weak_ptr<DbClientLockFree> weakPtr = shared_from_this();
    connPtr->setCloseCallback([weakPtr](const DbConnectionPtr &closeConnPtr) {
        // Erase the connection
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;

        for (auto iter = thisPtr->_connections.begin();
             iter != thisPtr->_connections.end();
             iter++)
        {
            if (closeConnPtr == *iter)
            {
                thisPtr->_connections.erase(iter);
                break;
            }
        }
        for (auto iter = thisPtr->_connectionHolders.begin();
             iter != thisPtr->_connectionHolders.end();
             iter++)
        {
            if (closeConnPtr == *iter)
            {
                thisPtr->_connectionHolders.erase(iter);
                break;
            }
        }

        thisPtr->_transSet.erase(closeConnPtr);
        // Reconnect after 1 second
        thisPtr->_loop->runAfter(1, [weakPtr] {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            thisPtr->_connectionHolders.push_back(thisPtr->newConnection());
        });
    });
    connPtr->setOkCallback([weakPtr](const DbConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        thisPtr->_connections.push_back(okConnPtr);
        thisPtr->handleNewTask(okConnPtr);
    });
    std::weak_ptr<DbConnection> weakConnPtr = connPtr;
    connPtr->setIdleCallback([weakPtr, weakConnPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        auto connPtr = weakConnPtr.lock();
        if (!connPtr)
            return;
        thisPtr->handleNewTask(connPtr);
    });
    // std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}
