//
// Created by antao on 2018/6/22.
//
#include "PgClientImpl.h"
#include "PgTransactionImpl.h"
#include "PgConnection.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/DbClient.h>
#include <libpq-fe.h>
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

PgConnectionPtr PgClientImpl::newConnection(trantor::EventLoop *loop)
{
    //std::cout<<"newConn"<<std::endl;
    auto connPtr = std::make_shared<PgConnection>(loop, _connInfo);
    connPtr->setCloseCallback([=](const PgConnectionPtr &closeConnPtr) {
        //std::cout<<"Conn closed!"<<closeConnPtr<<std::endl;
        sleep(1);
        std::lock_guard<std::mutex> guard(_connectionsMutex);
        _readyConnections.erase(closeConnPtr);
        _busyConnections.erase(closeConnPtr);
        assert(_connections.find(closeConnPtr) != _connections.end());
        _connections.erase(closeConnPtr);
        _connections.insert(newConnection(loop));
        //std::cout<<"Conn closed!end"<<std::endl;
    });
    connPtr->setOkCallback([=](const PgConnectionPtr &okConnPtr) {
        LOG_TRACE << "postgreSQL connected!";
        {
            std::lock_guard<std::mutex> guard(_connectionsMutex);
            _readyConnections.insert(okConnPtr);
        }

        _condConnectionReady.notify_one();
    });
    //std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}
PgClientImpl::PgClientImpl(const std::string &connInfo, const size_t connNum)
    : _connInfo(connInfo),
      _connectNum(connNum)
{
    assert(connNum > 0);
    _loopThread = std::thread([=]() {
        _loopPtr = std::unique_ptr<trantor::EventLoop>(new trantor::EventLoop);
        ioLoop();
    });
}
void PgClientImpl::ioLoop()
{

    for (size_t i = 0; i < _connectNum; i++)
    {
        _connections.insert(newConnection(_loopPtr.get()));
    }
    _loopPtr->loop();
}

PgClientImpl::~PgClientImpl()
{
    _stop = true;
    _loopPtr->quit();
    if (_loopThread.joinable())
        _loopThread.join();
}

void PgClientImpl::execSql(const PgConnectionPtr &conn,
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
    std::weak_ptr<PgConnection> weakConn = conn;
    conn->execSql(sql, paraNum, parameters, length, format,
                  cb, exceptCallback,
                  [=]() -> void {
                      {
                          auto connPtr = weakConn.lock();
                          if (!connPtr)
                              return;
                          {
                              std::lock_guard<std::mutex> guard(_bufferMutex);
                              if (_sqlCmdBuffer.size() > 0)
                              {
                                  auto cmd = _sqlCmdBuffer.front();
                                  _sqlCmdBuffer.pop_front();
                                  _loopPtr->queueInLoop([=]() {
                                      std::vector<const char *> paras;
                                      std::vector<int> lens;
                                      for (auto &p : cmd._parameters)
                                      {
                                          paras.push_back(p.c_str());
                                          lens.push_back(p.length());
                                      }
                                      execSql(connPtr, cmd._sql, cmd._paraNum, paras, lens, cmd._format, cmd._cb, cmd._exceptCb);
                                  });

                                  return;
                              }
                          }
                          {
                              std::lock_guard<std::mutex> guard(_connectionsMutex);
                              _busyConnections.erase(connPtr);
                              _readyConnections.insert(connPtr);
                          }
                          _condConnectionReady.notify_one();
                      }
                  });
}
void PgClientImpl::execSql(const std::string &sql,
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
    PgConnectionPtr conn;
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
    for (size_t i = 0; i < parameters.size(); i++)
    {
        //LOG_TRACE << "parameters[" << i << "]=" << (size_t)(parameters[i]);
        cmd._parameters.push_back(std::string(parameters[i], length[i]));
    }
    cmd._format = format;
    cmd._cb = cb;
    cmd._exceptCb = exceptCb;
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        _sqlCmdBuffer.push_back(std::move(cmd));
    }
}

std::string PgClientImpl::replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const
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

std::shared_ptr<Transaction> PgClientImpl::newTransaction()
{
    PgConnectionPtr conn;
    {
        std::unique_lock<std::mutex> lock(_connectionsMutex);

        _condConnectionReady.wait(lock, [this]() {
            return _readyConnections.size() > 0;
        });

        auto iter = _readyConnections.begin();
        _busyConnections.insert(*iter);
        conn = *iter;
        _readyConnections.erase(iter);
    }
    auto trans = std::shared_ptr<PgTransactionImpl>(new PgTransactionImpl(conn, [=]() {
        {
            std::lock_guard<std::mutex> guard(_bufferMutex);
            if (_sqlCmdBuffer.size() > 0)
            {
                auto cmd = _sqlCmdBuffer.front();
                _sqlCmdBuffer.pop_front();
                _loopPtr->queueInLoop([=]() {
                    std::vector<const char *> paras;
                    std::vector<int> lens;
                    for (auto &p : cmd._parameters)
                    {
                        paras.push_back(p.c_str());
                        lens.push_back(p.length());
                    }
                    execSql(conn, cmd._sql, cmd._paraNum, paras, lens, cmd._format, cmd._cb, cmd._exceptCb);
                });

                return;
            }
        }
        {
            std::lock_guard<std::mutex> lock(_connectionsMutex);
            _readyConnections.insert(conn);
        }
        _condConnectionReady.notify_one();
    }));
    trans->doBegin();
    return trans;
}
