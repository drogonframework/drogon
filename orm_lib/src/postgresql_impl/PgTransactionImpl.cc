/**
 *
 *  PgTransactionImpl.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include "PgTransactionImpl.h"
#include <trantor/utils/Logger.h>

using namespace drogon::orm;

PgTransactionImpl::PgTransactionImpl(const PgConnectionPtr &connPtr,
                                     const std::function<void()> &usedUpCallback)
    : _connectionPtr(connPtr),
      _usedUpCallback(usedUpCallback),
      _loop(connPtr->loop())
{
}
PgTransactionImpl::~PgTransactionImpl()
{
    LOG_TRACE << "Destruct";
    assert(!_isWorking);
    if (!_isCommitedOrRolledback)
    {
        auto cb = _usedUpCallback;
        auto loop = _connectionPtr->loop();
        auto conn = _connectionPtr;
        loop->queueInLoop([conn, cb]() {
            conn->execSql("commit",
                          0,
                          std::vector<const char *>(),
                          std::vector<int>(),
                          std::vector<int>(),
                          [](const Result &r) {
                              LOG_TRACE << "Transaction commited!";
                          },
                          [](const std::exception_ptr &ePtr) {

                          },
                          [cb]() {
                              if (cb)
                              {
                                  cb();
                              }
                          });
        });
    }
}
void PgTransactionImpl::execSql(const std::string &sql,
                                size_t paraNum,
                                const std::vector<const char *> &parameters,
                                const std::vector<int> &length,
                                const std::vector<int> &format,
                                const ResultCallback &rcb,
                                const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    auto thisPtr = shared_from_this();
    _loop->queueInLoop([thisPtr, sql, paraNum, parameters, length, format, rcb, exceptCallback]() {
        if (!thisPtr->_isCommitedOrRolledback)
        {
            if (!thisPtr->_isWorking)
            {
                thisPtr->_isWorking = true;
                thisPtr->_connectionPtr->execSql(sql,
                                                 paraNum,
                                                 parameters,
                                                 length,
                                                 format,
                                                 rcb,
                                                 [exceptCallback, thisPtr](const std::exception_ptr &ePtr) {
                                                     thisPtr->rollback();
                                                     if(exceptCallback)
                                                        exceptCallback(ePtr);
                                                 },
                                                 [thisPtr]() {
                                                     thisPtr->execNewTask();
                                                 });
            }
            else
            {
                //push sql cmd to buffer;
                SqlCmd cmd;
                cmd._sql = sql;
                cmd._paraNum = paraNum;
                for (size_t i = 0; i < parameters.size(); i++)
                {
                    //LOG_TRACE << "parameters[" << i << "]=" << (size_t)(parameters[i]);
                    cmd._parameters.push_back(std::string(parameters[i], length[i]));
                }
                cmd._format = format;
                cmd._cb = rcb;
                cmd._exceptCb = exceptCallback;
                thisPtr->_sqlCmdBuffer.push_back(std::move(cmd));
            }
        }
        else
        {
            //The transaction has been rolled back;
            try
            {
                throw TransactionRollback("The transaction has been rolled back");
            }
            catch (...)
            {
                exceptCallback(std::current_exception());
            }
        }
    });
}

void PgTransactionImpl::rollback()
{
    auto thisPtr = shared_from_this();

    _loop->runInLoop([thisPtr]() {
        if (thisPtr->_isCommitedOrRolledback)
            return;
        auto clearupCb = [thisPtr]() {
            thisPtr->_isCommitedOrRolledback = true;
            if (thisPtr->_usedUpCallback)
            {
                thisPtr->_usedUpCallback();
                thisPtr->_usedUpCallback = decltype(thisPtr->_usedUpCallback)();
            }
        };
        if (thisPtr->_isWorking)
        {
            //push sql cmd to buffer;
            SqlCmd cmd;
            cmd._sql = "rollback";
            cmd._paraNum = 0;
            cmd._cb = [clearupCb](const Result &r) {
                LOG_TRACE << "Transaction roll back!";
                clearupCb();
            };
            cmd._exceptCb = [clearupCb](const std::exception_ptr &ePtr) {
                clearupCb();
            };
            //Rollback cmd should be executed firstly, so we push it in front of the list
            thisPtr->_sqlCmdBuffer.push_front(std::move(cmd));
            return;
        }
        thisPtr->_isWorking = true;
        thisPtr
            ->_connectionPtr
            ->execSql("rollback",
                      0,
                      std::vector<const char *>(),
                      std::vector<int>(),
                      std::vector<int>(),
                      [clearupCb](const Result &r) {
                          LOG_TRACE << "Transaction roll back!";
                          clearupCb();
                      },
                      [clearupCb](const std::exception_ptr &ePtr) {
                          clearupCb();
                      },
                      [thisPtr]() {
                          thisPtr->execNewTask();
                      });
    });
}

void PgTransactionImpl::execNewTask()
{
    _loop->assertInLoopThread();
    assert(_isWorking);
    if (!_isCommitedOrRolledback)
    {
        auto thisPtr = shared_from_this();
        if (_sqlCmdBuffer.size() > 0)
        {
            auto cmd = _sqlCmdBuffer.front();
            _sqlCmdBuffer.pop_front();

            std::vector<const char *> paras;
            std::vector<int> lens;
            for (auto &p : cmd._parameters)
            {
                paras.push_back(p.c_str());
                lens.push_back(p.length());
            }
            auto conn = _connectionPtr;

            _loop->queueInLoop([=]() {
                conn->execSql(cmd._sql,
                              cmd._paraNum,
                              paras,
                              lens,
                              cmd._format,
                              cmd._cb,
                              [cmd, thisPtr](const std::exception_ptr &ePtr) {
                                  thisPtr->rollback();
                                  if (cmd._exceptCb)
                                      cmd._exceptCb(ePtr);
                              },
                              [thisPtr]() {
                                  thisPtr->execNewTask();
                              });
            });

            return;
        }
        _isWorking = false;
    }
    else
    {
        _isWorking = false;
        if (_sqlCmdBuffer.size() > 0)
        {
            try
            {
                throw TransactionRollback("The transaction has been rolled back");
            }
            catch (...)
            {
                for (auto &cmd : _sqlCmdBuffer)
                {
                    if (cmd._exceptCb)
                    {
                        cmd._exceptCb(std::current_exception());
                    }
                }
            }
            _sqlCmdBuffer.clear();
        }
    }
}

void PgTransactionImpl::doBegin()
{
    auto thisPtr = shared_from_this();
    _loop->queueInLoop([thisPtr]() {
        assert(!thisPtr->_isWorking);
        assert(!thisPtr->_isCommitedOrRolledback);
        thisPtr->_isWorking = true;
        thisPtr->_connectionPtr->execSql("begin",
                                         0,
                                         std::vector<const char *>(),
                                         std::vector<int>(),
                                         std::vector<int>(),
                                         [](const Result &r) {
                                             LOG_TRACE << "Transaction begin!";
                                         },
                                         [thisPtr](const std::exception_ptr &ePtr) {
                                             thisPtr->_isCommitedOrRolledback = true;

                                             if (thisPtr->_usedUpCallback)
                                             {
                                                 thisPtr->_usedUpCallback();
                                             }
                                         },
                                         [thisPtr]() {
                                             thisPtr->execNewTask();
                                         });
    });
}

std::string PgTransactionImpl::replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const
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
