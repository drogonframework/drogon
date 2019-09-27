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

TransactionImpl::TransactionImpl(
    ClientType type,
    const DbConnectionPtr &connPtr,
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
    assert(_sqlCmdBuffer.empty());
    if (!_isCommitedOrRolledback)
    {
        auto loop = _connectionPtr->loop();
        loop->queueInLoop([conn = _connectionPtr,
                           ucb = std::move(_usedUpCallback),
                           commitCb = std::move(_commitCallback)]() {
            conn->setIdleCallback([ucb = std::move(ucb)]() {
                if (ucb)
                    ucb();
            });
            conn->execSql(
                "commit",
                0,
                std::vector<const char *>(),
                std::vector<int>(),
                std::vector<int>(),
                [commitCb](const Result &) {
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
                            LOG_ERROR << "Transaction submission failed:"
                                      << e.base().what();
                            commitCb(false);
                        }
                    }
                });
        });
    }
    else
    {
        if (_usedUpCallback)
        {
            _usedUpCallback();
        }
    }
}
void TransactionImpl::execSqlInLoop(
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    _loop->assertInLoopThread();
    if (!_isCommitedOrRolledback)
    {
        auto thisPtr = shared_from_this();
        if (!_isWorking)
        {
            _isWorking = true;
            _thisPtr = thisPtr;
            _connectionPtr->execSql(std::move(sql),
                                    paraNum,
                                    std::move(parameters),
                                    std::move(length),
                                    std::move(format),
                                    std::move(rcb),
                                    [exceptCallback,
                                     thisPtr](const std::exception_ptr &ePtr) {
                                        thisPtr->rollback();
                                        if (exceptCallback)
                                            exceptCallback(ePtr);
                                    });
        }
        else
        {
            // push sql cmd to buffer;
            SqlCmd cmd;
            cmd._sql = std::move(sql);
            cmd._paraNum = paraNum;
            cmd._parameters = std::move(parameters);
            cmd._length = std::move(length);
            cmd._format = std::move(format);
            cmd._cb = std::move(rcb);
            cmd._exceptCb = std::move(exceptCallback);
            cmd._thisPtr = thisPtr;
            thisPtr->_sqlCmdBuffer.push_back(std::move(cmd));
        }
    }
    else
    {
        // The transaction has been rolled back;
        try
        {
            throw TransactionRollback("The transaction has been rolled back");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
    }
}

void TransactionImpl::rollback()
{
    auto thisPtr = shared_from_this();

    _loop->runInLoop([thisPtr]() {
        if (thisPtr->_isCommitedOrRolledback)
            return;
        if (thisPtr->_isWorking)
        {
            // push sql cmd to buffer;
            SqlCmd cmd;
            cmd._sql = "rollback";
            cmd._paraNum = 0;
            cmd._cb = [thisPtr](const Result &) {
                LOG_DEBUG << "Transaction roll back!";
                thisPtr->_isCommitedOrRolledback = true;
            };
            cmd._exceptCb = [thisPtr](const std::exception_ptr &) {
                // clearupCb();
                thisPtr->_isCommitedOrRolledback = true;
                LOG_ERROR << "Transaction rool back error";
            };
            cmd._isRollbackCmd = true;
            // Rollback cmd should be executed firstly, so we push it in front
            // of the list
            thisPtr->_sqlCmdBuffer.push_front(std::move(cmd));
            return;
        }
        thisPtr->_isWorking = true;
        thisPtr->_thisPtr = thisPtr;
        thisPtr->_connectionPtr->execSql(
            "rollback",
            0,
            std::vector<const char *>(),
            std::vector<int>(),
            std::vector<int>(),
            [thisPtr](const Result &) {
                LOG_TRACE << "Transaction roll back!";
                thisPtr->_isCommitedOrRolledback = true;
                // clearupCb();
            },
            [thisPtr](const std::exception_ptr &) {
                // clearupCb();
                LOG_ERROR << "Transaction rool back error";
                thisPtr->_isCommitedOrRolledback = true;
            });
    });
}

void TransactionImpl::execNewTask()
{
    _loop->assertInLoopThread();
    _thisPtr.reset();
    assert(_isWorking);
    if (!_isCommitedOrRolledback)
    {
        auto thisPtr = shared_from_this();
        if (!_sqlCmdBuffer.empty())
        {
            auto cmd = std::move(_sqlCmdBuffer.front());
            _sqlCmdBuffer.pop_front();
            auto conn = _connectionPtr;
            conn->execSql(
                std::move(cmd._sql),
                cmd._paraNum,
                std::move(cmd._parameters),
                std::move(cmd._length),
                std::move(cmd._format),
                [callback = std::move(cmd._cb), cmd, thisPtr](const Result &r) {
                    if (cmd._isRollbackCmd)
                    {
                        thisPtr->_isCommitedOrRolledback = true;
                    }
                    if (callback)
                        callback(r);
                },
                [cmd, thisPtr](const std::exception_ptr &ePtr) {
                    if (!cmd._isRollbackCmd)
                        thisPtr->rollback();
                    else
                    {
                        thisPtr->_isCommitedOrRolledback = true;
                    }
                    if (cmd._exceptCb)
                        cmd._exceptCb(ePtr);
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
                throw TransactionRollback(
                    "The transaction has been rolled back");
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
        if (_usedUpCallback)
        {
            _usedUpCallback();
        }
    }
}

void TransactionImpl::doBegin()
{
    _loop->queueInLoop([thisPtr = shared_from_this()]() {
        std::weak_ptr<TransactionImpl> weakPtr = thisPtr;
        thisPtr->_connectionPtr->setIdleCallback([weakPtr]() {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            thisPtr->execNewTask();
        });
        assert(!thisPtr->_isWorking);
        assert(!thisPtr->_isCommitedOrRolledback);
        thisPtr->_isWorking = true;
        thisPtr->_thisPtr = thisPtr;
        thisPtr->_connectionPtr->execSql(
            "begin",
            0,
            std::vector<const char *>(),
            std::vector<int>(),
            std::vector<int>(),
            [](const Result &) { LOG_TRACE << "Transaction begin!"; },
            [thisPtr](const std::exception_ptr &) {
                LOG_ERROR << "Error occurred in transaction begin";
                thisPtr->_isCommitedOrRolledback = true;
            });
    });
}
