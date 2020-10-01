/**
 *
 *  @file DbClientImpl.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbClientImpl.h"
#include "DbConnection.h"
#include <drogon/config.h>
#include <drogon/utils/string_view.h>
#if USE_POSTGRESQL
#include "postgresql_impl/PgConnection.h"
#endif
#if USE_MYSQL
#include "mysql_impl/MysqlConnection.h"
#endif
#if USE_SQLITE3
#include "sqlite3_impl/Sqlite3Connection.h"
#endif
#include "TransactionImpl.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdio.h>
#ifndef _WIN32
#include <sys/select.h>
#include <unistd.h>
#endif
#include <thread>
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <unordered_set>
#include <vector>

using namespace drogon;
using namespace drogon::orm;

DbClientImpl::DbClientImpl(const std::string &connInfo,
                           const size_t connNum,
                           ClientType type)
    : connectionsNumber_(connNum),
      loops_(type == ClientType::Sqlite3
                 ? 1
                 : (connNum < std::thread::hardware_concurrency()
                        ? connNum
                        : std::thread::hardware_concurrency()),
             "DbLoop")
{
    type_ = type;
    connectionInfo_ = connInfo;
    LOG_TRACE << "type=" << (int)type;
    // LOG_DEBUG << loops_.getLoopNum();
    assert(connNum > 0);
    loops_.start();
    if (type == ClientType::PostgreSQL)
    {
        std::thread([this]() {
            for (size_t i = 0; i < connectionsNumber_; ++i)
            {
                auto loop = loops_.getNextLoop();
                loop->runInLoop([this, loop]() {
                    std::lock_guard<std::mutex> lock(connectionsMutex_);
                    connections_.insert(newConnection(loop));
                });
            }
        }).detach();
    }
    else if (type == ClientType::Mysql)
    {
        std::thread([this]() {
            for (size_t i = 0; i < connectionsNumber_; ++i)
            {
                auto loop = loops_.getNextLoop();
                loop->runAfter(0.1 * (i + 1), [this, loop]() {
                    std::lock_guard<std::mutex> lock(connectionsMutex_);
                    connections_.insert(newConnection(loop));
                });
            }
        }).detach();
    }
    else if (type == ClientType::Sqlite3)
    {
        sharedMutexPtr_ = std::make_shared<SharedMutex>();
        assert(sharedMutexPtr_);
        auto loop = loops_.getNextLoop();
        loop->runInLoop([this]() {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            for (size_t i = 0; i < connectionsNumber_; ++i)
            {
                connections_.insert(newConnection(nullptr));
            }
        });
    }
}

DbClientImpl::~DbClientImpl() noexcept
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto const &conn : connections_)
    {
        conn->disconnect();
    }
    connections_.clear();
    readyConnections_.clear();
    busyConnections_.clear();
}

void DbClientImpl::execSql(
    const DbConnectionPtr &conn,
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    assert(conn);
    conn->execSql(std::move(sql),
                  paraNum,
                  std::move(parameters),
                  std::move(length),
                  std::move(format),
                  std::move(rcb),
                  std::move(exceptCallback));
}
void DbClientImpl::execSql(
    const char *sql,
    size_t sqlLength,
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
    DbConnectionPtr conn;
    bool busy = false;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);

        if (readyConnections_.size() == 0)
        {
            if (sqlCmdBuffer_.size() > 200000)
            {
                // too many queries in buffer;
                busy = true;
            }
            else
            {
                // LOG_TRACE << "Push query to buffer";
                std::shared_ptr<SqlCmd> cmd =
                    std::make_shared<SqlCmd>(string_view{sql, sqlLength},
                                             paraNum,
                                             std::move(parameters),
                                             std::move(length),
                                             std::move(format),
                                             std::move(rcb),
                                             std::move(exceptCallback));
                sqlCmdBuffer_.push_back(std::move(cmd));
            }
        }
        else
        {
            auto iter = readyConnections_.begin();
            busyConnections_.insert(*iter);
            conn = *iter;
            readyConnections_.erase(iter);
        }
    }
    if (conn)
    {
        execSql(conn,
                string_view{sql, sqlLength},
                paraNum,
                std::move(parameters),
                std::move(length),
                std::move(format),
                std::move(rcb),
                std::move(exceptCallback));
        return;
    }
    if (busy)
    {
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
}
void DbClientImpl::newTransactionAsync(
    const std::function<void(const std::shared_ptr<Transaction> &)> &callback)
{
    DbConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (!readyConnections_.empty())
        {
            auto iter = readyConnections_.begin();
            busyConnections_.insert(*iter);
            conn = *iter;
            readyConnections_.erase(iter);
        }
        else
        {
            transCallbacks_.push(callback);
        }
    }
    if (conn)
    {
        makeTrans(conn,
                  std::function<void(const std::shared_ptr<Transaction> &)>(
                      callback));
    }
}
void DbClientImpl::makeTrans(
    const DbConnectionPtr &conn,
    std::function<void(const std::shared_ptr<Transaction> &)> &&callback)
{
    std::weak_ptr<DbClientImpl> weakThis = shared_from_this();
    auto trans = std::shared_ptr<TransactionImpl>(new TransactionImpl(
        type_, conn, std::function<void(bool)>(), [weakThis, conn]() {
            auto thisPtr = weakThis.lock();
            if (!thisPtr)
                return;
            if (conn->status() == ConnectStatus::Bad)
            {
                return;
            }
            {
                std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
                if (thisPtr->connections_.find(conn) ==
                    thisPtr->connections_.end())
                {
                    // connection is broken and removed
                    assert(thisPtr->busyConnections_.find(conn) ==
                               thisPtr->busyConnections_.end() &&
                           thisPtr->readyConnections_.find(conn) ==
                               thisPtr->readyConnections_.end());

                    return;
                }
            }
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
                thisPtr->handleNewTask(conn);
            });
        }));
    trans->doBegin();
    conn->loop()->queueInLoop(
        [callback = std::move(callback), trans]() { callback(trans); });
}
std::shared_ptr<Transaction> DbClientImpl::newTransaction(
    const std::function<void(bool)> &commitCallback)
{
    std::promise<std::shared_ptr<Transaction>> pro;
    auto f = pro.get_future();
    newTransactionAsync([&pro](const std::shared_ptr<Transaction> &trans) {
        pro.set_value(trans);
    });
    auto trans = f.get();
    trans->setCommitCallback(commitCallback);
    return trans;
}

void DbClientImpl::handleNewTask(const DbConnectionPtr &connPtr)
{
    std::function<void(const std::shared_ptr<Transaction> &)> transCallback;
    std::shared_ptr<SqlCmd> cmd;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);
        if (!transCallbacks_.empty())
        {
            transCallback = std::move(transCallbacks_.front());
            transCallbacks_.pop();
        }
        else if (!sqlCmdBuffer_.empty())
        {
            cmd = std::move(sqlCmdBuffer_.front());
            sqlCmdBuffer_.pop_front();
        }
        else
        {
            // Connection is idle, put it into the readyConnections_ set;
            busyConnections_.erase(connPtr);
            readyConnections_.insert(connPtr);
        }
    }
    if (transCallback)
    {
        makeTrans(connPtr, std::move(transCallback));
        return;
    }
    if (cmd)
    {
        execSql(connPtr,
                std::move(cmd->sql_),
                cmd->parametersNumber_,
                std::move(cmd->parameters_),
                std::move(cmd->lengths_),
                std::move(cmd->formats_),
                std::move(cmd->callback_),
                std::move(cmd->exceptionCallback_));
        return;
    }
}

DbConnectionPtr DbClientImpl::newConnection(trantor::EventLoop *loop)
{
    DbConnectionPtr connPtr;
    if (type_ == ClientType::PostgreSQL)
    {
#if USE_POSTGRESQL
        connPtr = std::make_shared<PgConnection>(loop, connectionInfo_);
#else
        return nullptr;
#endif
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        connPtr = std::make_shared<MysqlConnection>(loop, connectionInfo_);
#else
        return nullptr;
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
#if USE_SQLITE3
        connPtr = std::make_shared<Sqlite3Connection>(loop,
                                                      connectionInfo_,
                                                      sharedMutexPtr_);
#else
        return nullptr;
#endif
    }
    else
    {
        return nullptr;
    }

    std::weak_ptr<DbClientImpl> weakPtr = shared_from_this();
    connPtr->setCloseCallback([weakPtr](const DbConnectionPtr &closeConnPtr) {
        // Erase the connection
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
            thisPtr->readyConnections_.erase(closeConnPtr);
            thisPtr->busyConnections_.erase(closeConnPtr);
            assert(thisPtr->connections_.find(closeConnPtr) !=
                   thisPtr->connections_.end());
            thisPtr->connections_.erase(closeConnPtr);
        }
        // Reconnect after 1 second
        auto loop = closeConnPtr->loop();
        loop->runAfter(1, [weakPtr, loop] {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
            thisPtr->connections_.insert(thisPtr->newConnection(loop));
        });
    });
    connPtr->setOkCallback([weakPtr](const DbConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
            thisPtr->busyConnections_.insert(
                okConnPtr);  // For new connections, this sentence is necessary
        }
        thisPtr->handleNewTask(okConnPtr);
    });
    std::weak_ptr<DbConnection> weakConn = connPtr;
    connPtr->setIdleCallback([weakPtr, weakConn]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        auto connPtr = weakConn.lock();
        if (!connPtr)
            return;
        thisPtr->handleNewTask(connPtr);
    });
    // std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}

bool DbClientImpl::hasAvailableConnections() const noexcept
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return (!readyConnections_.empty()) || (!busyConnections_.empty());
}