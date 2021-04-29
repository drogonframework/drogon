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
#include "../../lib/src/TaskTimeoutFlag.h"
#include <drogon/utils/string_view.h>
#include <trantor/utils/Logger.h>

using namespace drogon::orm;
using namespace drogon;

TransactionImpl::TransactionImpl(
    ClientType type,
    const DbConnectionPtr &connPtr,
    const std::function<void(bool)> &commitCallback,
    const std::function<void()> &usedUpCallback)
    : connectionPtr_(connPtr),
      usedUpCallback_(usedUpCallback),
      loop_(connPtr->loop()),
      commitCallback_(commitCallback)
{
    type_ = type;
}
TransactionImpl::~TransactionImpl()
{
    LOG_TRACE << "Destruct";
    assert(sqlCmdBuffer_.empty());
    if (!isCommitedOrRolledback_)
    {
        auto loop = connectionPtr_->loop();
        loop->queueInLoop([conn = connectionPtr_,
                           ucb = std::move(usedUpCallback_),
                           commitCb = std::move(commitCallback_)]() {
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
        if (usedUpCallback_)
        {
            usedUpCallback_();
        }
    }
}
void TransactionImpl::execSqlInLoop(
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    loop_->assertInLoopThread();
    if (!isCommitedOrRolledback_)
    {
        if (timeout_ > 0.0)
        {
            execSqlInLoopWithTimeout(std::move(sql),
                                     paraNum,
                                     std::move(parameters),
                                     std::move(length),
                                     std::move(format),
                                     std::move(rcb),
                                     std::move(exceptCallback));
            return;
        }
        auto thisPtr = shared_from_this();
        if (!isWorking_)
        {
            isWorking_ = true;
            thisPtr_ = thisPtr;
            connectionPtr_->execSql(std::move(sql),
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
            auto cmdPtr = std::make_shared<SqlCmd>();
            cmdPtr->sql_ = std::move(sql);
            cmdPtr->parametersNumber_ = paraNum;
            cmdPtr->parameters_ = std::move(parameters);
            cmdPtr->lengths_ = std::move(length);
            cmdPtr->formats_ = std::move(format);
            cmdPtr->callback_ = std::move(rcb);
            cmdPtr->exceptionCallback_ = std::move(exceptCallback);
            cmdPtr->thisPtr_ = thisPtr;
            thisPtr->sqlCmdBuffer_.push_back(std::move(cmdPtr));
        }
    }
    else
    {
        // The transaction has been rolled back;
        auto exceptPtr = std::make_exception_ptr(
            TransactionRollback("The transaction has been rolled back"));
        exceptCallback(exceptPtr);
    }
}

void TransactionImpl::rollback()
{
    auto thisPtr = shared_from_this();

    loop_->runInLoop([thisPtr]() {
        if (thisPtr->isCommitedOrRolledback_)
            return;
        if (thisPtr->isWorking_)
        {
            // push sql cmd to buffer;
            auto cmdPtr = std::make_shared<SqlCmd>();
            cmdPtr->sql_ = "rollback";
            cmdPtr->parametersNumber_ = 0;
            cmdPtr->callback_ = [thisPtr](const Result &) {
                LOG_DEBUG << "Transaction roll back!";
                thisPtr->isCommitedOrRolledback_ = true;
            };
            cmdPtr->exceptionCallback_ = [thisPtr](const std::exception_ptr &) {
                // clearupCb();
                thisPtr->isCommitedOrRolledback_ = true;
                LOG_ERROR << "Transaction rool back error";
            };
            cmdPtr->isRollbackCmd_ = true;
            // Rollback cmd should be executed firstly, so we push it in front
            // of the list
            thisPtr->sqlCmdBuffer_.push_front(std::move(cmdPtr));
            return;
        }
        thisPtr->isWorking_ = true;
        thisPtr->thisPtr_ = thisPtr;
        thisPtr->connectionPtr_->execSql(
            "rollback",
            0,
            std::vector<const char *>(),
            std::vector<int>(),
            std::vector<int>(),
            [thisPtr](const Result &) {
                LOG_TRACE << "Transaction roll back!";
                thisPtr->isCommitedOrRolledback_ = true;
                // clearupCb();
            },
            [thisPtr](const std::exception_ptr &) {
                // clearupCb();
                LOG_ERROR << "Transaction rool back error";
                thisPtr->isCommitedOrRolledback_ = true;
            });
    });
}

void TransactionImpl::execNewTask()
{
    loop_->assertInLoopThread();
    thisPtr_.reset();
    assert(isWorking_);
    if (!isCommitedOrRolledback_)
    {
        auto thisPtr = shared_from_this();
        if (!sqlCmdBuffer_.empty())
        {
            auto cmd = std::move(sqlCmdBuffer_.front());
            sqlCmdBuffer_.pop_front();
            auto conn = connectionPtr_;
            conn->execSql(
                std::move(cmd->sql_),
                cmd->parametersNumber_,
                std::move(cmd->parameters_),
                std::move(cmd->lengths_),
                std::move(cmd->formats_),
                [callback = std::move(cmd->callback_), cmd, thisPtr](
                    const Result &r) {
                    if (cmd->isRollbackCmd_)
                    {
                        thisPtr->isCommitedOrRolledback_ = true;
                    }
                    if (callback)
                        callback(r);
                },
                [cmd, thisPtr](const std::exception_ptr &ePtr) {
                    if (!cmd->isRollbackCmd_)
                        thisPtr->rollback();
                    else
                    {
                        thisPtr->isCommitedOrRolledback_ = true;
                    }
                    if (cmd->exceptionCallback_)
                        cmd->exceptionCallback_(ePtr);
                });
            return;
        }
        isWorking_ = false;
    }
    else
    {
        isWorking_ = false;
        if (!sqlCmdBuffer_.empty())
        {
            auto exceptPtr = std::make_exception_ptr(
                TransactionRollback("The transaction has been rolled back"));
            for (auto const &cmd : sqlCmdBuffer_)
            {
                if (cmd->exceptionCallback_)
                {
                    cmd->exceptionCallback_(exceptPtr);
                }
            }
            sqlCmdBuffer_.clear();
        }
        if (usedUpCallback_)
        {
            usedUpCallback_();
            usedUpCallback_ = std::function<void()>();
        }
    }
}

void TransactionImpl::doBegin()
{
    loop_->queueInLoop([thisPtr = shared_from_this()]() {
        std::weak_ptr<TransactionImpl> weakPtr = thisPtr;
        thisPtr->connectionPtr_->setIdleCallback([weakPtr]() {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            thisPtr->execNewTask();
        });
        assert(!thisPtr->isWorking_);
        assert(!thisPtr->isCommitedOrRolledback_);
        thisPtr->isWorking_ = true;
        thisPtr->thisPtr_ = thisPtr;
        thisPtr->connectionPtr_->execSql(
            "begin",
            0,
            std::vector<const char *>(),
            std::vector<int>(),
            std::vector<int>(),
            [](const Result &) { LOG_TRACE << "Transaction begin!"; },
            [thisPtr](const std::exception_ptr &) {
                LOG_ERROR << "Error occurred in transaction begin";
                thisPtr->isCommitedOrRolledback_ = true;
            });
    });
}

void TransactionImpl::execSqlInLoopWithTimeout(
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&ecb)
{
    auto thisPtr = shared_from_this();
    std::weak_ptr<TransactionImpl> weakPtr = thisPtr;
    auto commandPtr = std::make_shared<std::weak_ptr<SqlCmd>>();
    auto ecpPtr =
        std::make_shared<std::function<void(const std::exception_ptr &)>>(
            std::move(ecb));
    auto timeoutFlagPtr = std::make_shared<drogon::TaskTimeoutFlag>(
        loop_,
        std::chrono::duration<double>(timeout_),
        [commandPtr, weakPtr, ecpPtr]() {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            auto cmdPtr = (*commandPtr).lock();
            if (cmdPtr)
            {
                for (auto iter = thisPtr->sqlCmdBuffer_.begin();
                     iter != thisPtr->sqlCmdBuffer_.end();
                     ++iter)
                {
                    if (cmdPtr == *iter)
                    {
                        thisPtr->sqlCmdBuffer_.erase(iter);
                        break;
                    }
                }
            }
            thisPtr->rollback();
            if (*ecpPtr)
            {
                (*ecpPtr)(std::make_exception_ptr(
                    TimeoutError("SQL execution timeout")));
            }
        });
    auto resultCallback = [rcb = std::move(rcb),
                           timeoutFlagPtr](const drogon::orm::Result &result) {
        if (timeoutFlagPtr->done())
            return;
        rcb(result);
    };
    if (!isWorking_)
    {
        isWorking_ = true;
        thisPtr_ = thisPtr;
        connectionPtr_->execSql(std::move(sql),
                                paraNum,
                                std::move(parameters),
                                std::move(length),
                                std::move(format),
                                std::move(resultCallback),
                                [ecpPtr, timeoutFlagPtr, thisPtr](
                                    const std::exception_ptr &ePtr) {
                                    thisPtr->rollback();
                                    if (timeoutFlagPtr->done())
                                        return;
                                    if (*ecpPtr)
                                    {
                                        (*ecpPtr)(ePtr);
                                    }
                                });
    }
    else
    {
        // push sql cmd to buffer;
        auto cmdPtr = std::make_shared<SqlCmd>();
        cmdPtr->sql_ = std::move(sql);
        cmdPtr->parametersNumber_ = paraNum;
        cmdPtr->parameters_ = std::move(parameters);
        cmdPtr->lengths_ = std::move(length);
        cmdPtr->formats_ = std::move(format);
        cmdPtr->callback_ = std::move(resultCallback);
        cmdPtr->exceptionCallback_ =
            [ecpPtr, timeoutFlagPtr](const std::exception_ptr &ePtr) {
                if (timeoutFlagPtr->done())
                    return;
                if (*ecpPtr)
                {
                    (*ecpPtr)(ePtr);
                }
            };

        cmdPtr->thisPtr_ = thisPtr;
        thisPtr->sqlCmdBuffer_.push_back(cmdPtr);
        *commandPtr = cmdPtr;
    }
    timeoutFlagPtr->runTimer();
}
