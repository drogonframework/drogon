/**
 *
 *  TransactionImpl.cc
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

#include "TransactionImpl.h"
#include <trantor/utils/Logger.h>

using namespace drogon::orm;

TransactionImpl::TransactionImpl(ClientType type, const DbConnectionPtr &connPtr,
                                 const std::function<void(bool)> &commitCallback,
                                 const std::function<void()> &usedUpCallback)
    : _connectionPtr(connPtr),
      _usedUpCallback(usedUpCallback),
      _loop(connPtr->loop()),
      _commitCallback(commitCallback)
{
    _type = type;
}
TransactionImpl::~TransactionImpl()
{
    LOG_TRACE << "Destruct";
    assert(!_isWorking);
    if (!_isCommitedOrRolledback)
    {
        auto loop = _connectionPtr->loop();
        loop->queueInLoop([conn = _connectionPtr, ucb = std::move(_usedUpCallback), commitCb = std::move(_commitCallback)]() {
            conn->execSql("commit",
                          0,
                          std::vector<const char *>(),
                          std::vector<int>(),
                          std::vector<int>(),
                          [commitCb](const Result &r) {
                              LOG_TRACE << "Transaction commited!";
                              if (commitCb)
                              {
                                  commitCb(true);
                              }
                          },
                          [commitCb](const std::exception_ptr &ePtr) {
                              if (commitCb)
                              {
                                  try
                                  {
                                      std::rethrow_exception(ePtr);
                                  }
                                  catch (const DrogonDbException &e)
                                  {
                                      LOG_ERROR << "Transaction submission failed:" << e.base().what();
                                      commitCb(false);
                                  }
                              }
                          },
                          [ucb]() {
                              if (ucb)
                              {
                                  ucb();
                              }
                          });
        });
    }
}
void TransactionImpl::execSql(std::string &&sql,
                              size_t paraNum,
                              std::vector<const char *> &&parameters,
                              std::vector<int> &&length,
                              std::vector<int> &&format,
                              ResultCallback &&rcb,
                              std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    auto thisPtr = shared_from_this();
    _loop->queueInLoop([thisPtr, sql = std::move(sql), paraNum, parameters = std::move(parameters), length = std::move(length), format = std::move(format), rcb = std::move(rcb), exceptCallback = std::move(exceptCallback)]() mutable {
        if (!thisPtr->_isCommitedOrRolledback)
        {
            if (!thisPtr->_isWorking)
            {
                thisPtr->_isWorking = true;
                thisPtr->_connectionPtr->execSql(std::move(sql),
                                                 paraNum,
                                                 std::move(parameters),
                                                 std::move(length),
                                                 std::move(format),
                                                 std::move(rcb),
                                                 [exceptCallback, thisPtr](const std::exception_ptr &ePtr) {
                                                     thisPtr->rollback();
                                                     if (exceptCallback)
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
                cmd._sql = std::move(sql);
                cmd._paraNum = paraNum;
                cmd._parameters = std::move(parameters);
                cmd._length = std::move(length);
                cmd._format = std::move(format);
                cmd._cb = std::move(rcb);
                cmd._exceptCb = std::move(exceptCallback);
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

void TransactionImpl::rollback()
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

void TransactionImpl::execNewTask()
{
    _loop->assertInLoopThread();
    assert(_isWorking);
    if (!_isCommitedOrRolledback)
    {
        auto thisPtr = shared_from_this();
        if (!_sqlCmdBuffer.empty())
        {
            auto cmd = _sqlCmdBuffer.front();
            _sqlCmdBuffer.pop_front();

            auto conn = _connectionPtr;

            _loop->queueInLoop([=]() mutable {
                conn->execSql(std::move(cmd._sql),
                              cmd._paraNum,
                              std::move(cmd._parameters),
                              std::move(cmd._length),
                              std::move(cmd._format),
                              std::move(cmd._cb),
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
        if (!_sqlCmdBuffer.empty())
        {
            try
            {
                throw TransactionRollback("The transaction has been rolled back");
            }
            catch (...)
            {
                for (auto const &cmd : _sqlCmdBuffer)
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

void TransactionImpl::doBegin()
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
