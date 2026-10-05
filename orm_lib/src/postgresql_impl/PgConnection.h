/**
 *
 *  PgConnection.h
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

#pragma once

#include "../DbConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/orm/Exception.h>
#include <libpq-fe.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <string_view>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <set>

namespace drogon
{
namespace orm
{
namespace detail
{
inline constexpr std::string_view pgNotNullViolationState{"23502"};
inline constexpr std::string_view pgForeignKeyViolationState{"23503"};
inline constexpr std::string_view pgUniqueViolationState{"23505"};
inline constexpr std::string_view pgCheckViolationState{"23514"};
inline constexpr std::string_view pgDataExceptionClass{"22"};
inline constexpr std::string_view pgIntegrityConstraintViolationClass{"23"};
inline constexpr std::string_view pgInsufficientPrivilegeState{"42501"};
inline constexpr std::string_view pgOutOfMemoryState{"53200"};
inline constexpr std::string_view pgDiskFullState{"53100"};
inline constexpr std::string_view pgDefaultErrorMessage{
    "PostgreSQL query failed"};

inline bool hasPgSqlStateClass(std::string_view state,
                               std::string_view stateClass)
{
    return state.compare(0, stateClass.size(), stateClass) == 0;
}

inline std::exception_ptr makePgError(PGresult *result,
                                      std::string_view query)
{
    const char *sqlState = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    const char *message =
        PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
    if (!message || !*message)
        message = PQresultErrorMessage(result);
    if (!message || !*message)
        message = pgDefaultErrorMessage.data();

    const std::string errorMessage{message};
    const std::string queryString{query};
    const std::string_view state{sqlState ? sqlState : ""};

    if (state == pgNotNullViolationState)
        return std::make_exception_ptr(
            NotNullViolation(errorMessage, queryString, sqlState));
    if (state == pgForeignKeyViolationState)
        return std::make_exception_ptr(
            ForeignKeyViolation(errorMessage, queryString, sqlState));
    if (state == pgUniqueViolationState)
        return std::make_exception_ptr(
            UniqueViolation(errorMessage, queryString, sqlState));
    if (state == pgCheckViolationState)
        return std::make_exception_ptr(
            CheckViolation(errorMessage, queryString, sqlState));
    if (hasPgSqlStateClass(state, pgDataExceptionClass))
        return std::make_exception_ptr(
            DataException(errorMessage, queryString, sqlState));
    if (hasPgSqlStateClass(state, pgIntegrityConstraintViolationClass))
        return std::make_exception_ptr(
            IntegrityConstraintViolation(errorMessage, queryString, sqlState));
    if (state == pgInsufficientPrivilegeState)
        return std::make_exception_ptr(
            InsufficientPrivilege(errorMessage, queryString, sqlState));
    if (state == pgOutOfMemoryState)
        return std::make_exception_ptr(
            OutOfMemory(errorMessage, queryString, sqlState));
    if (state == pgDiskFullState)
        return std::make_exception_ptr(
            DiskFull(errorMessage, queryString, sqlState));

    return std::make_exception_ptr(
        SqlError(errorMessage, queryString, sqlState));
}
}  // namespace detail

class PgConnection;
using PgConnectionPtr = std::shared_ptr<PgConnection>;

class PgConnection : public DbConnection,
                     public std::enable_shared_from_this<PgConnection>
{
  public:
    using MessageCallback =
        std::function<void(const std::string &, const std::string &)>;
    PgConnection(trantor::EventLoop *loop,
                 const std::string &connInfo,
                 bool autoBatch);

    void init() override;

    void execSql(std::string_view &&sql,
                 size_t paraNum,
                 std::vector<const char *> &&parameters,
                 std::vector<int> &&length,
                 std::vector<int> &&format,
                 ResultCallback &&rcb,
                 std::function<void(const std::exception_ptr &)>
                     &&exceptCallback) override
    {
        if (loop_->isInLoopThread())
        {
            execSqlInLoop(std::move(sql),
                          paraNum,
                          std::move(parameters),
                          std::move(length),
                          std::move(format),
                          std::move(rcb),
                          std::move(exceptCallback));
        }
        else
        {
            auto thisPtr = shared_from_this();
            loop_->queueInLoop(
                [thisPtr,
                 sql = std::move(sql),
                 paraNum,
                 parameters = std::move(parameters),
                 length = std::move(length),
                 format = std::move(format),
                 rcb = std::move(rcb),
                 exceptCallback = std::move(exceptCallback)]() mutable {
                    thisPtr->execSqlInLoop(std::move(sql),
                                           paraNum,
                                           std::move(parameters),
                                           std::move(length),
                                           std::move(format),
                                           std::move(rcb),
                                           std::move(exceptCallback));
                });
        }
    }

    void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) override;

    void disconnect() override;

    const std::shared_ptr<PGconn> &pgConn() const
    {
        return connectionPtr_;
    }

    void setMessageCallback(MessageCallback cb)
    {
        messageCallback_ = std::move(cb);
    }

  private:
    std::shared_ptr<PGconn> connectionPtr_;
    trantor::Channel channel_;
    bool isPreparingStatement_{false};
    size_t preparedStatementsID_{0};

    std::string newStmtName()
    {
        loop_->assertInLoopThread();
        return std::to_string(++preparedStatementsID_);
    }

    void handleRead();
    void pgPoll();
    void handleClosed();

    void execSqlInLoop(
        std::string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback);
    void doAfterPreparing();
    std::string statementName_;
    int parametersNumber_{0};
    std::vector<const char *> parameters_;
    std::vector<int> lengths_;
    std::vector<int> formats_;
    int flush();
    void handleFatalError(PGresult *result = nullptr);
    std::set<std::string> preparedStatements_;
    std::string_view sql_;
#if LIBPQ_SUPPORTS_BATCH_MODE
    void handleFatalError(bool clearAll,
                          bool isAbortPipeline = false,
                          PGresult *result = nullptr);
    std::list<std::shared_ptr<SqlCmd>> batchCommandsForWaitingResults_;
    std::deque<std::shared_ptr<SqlCmd>> batchSqlCommands_;
    void sendBatchedSql();
    int sendBatchEnd();
    bool sendBatchEnd_{false};
    bool autoBatch_{false};
    unsigned int batchCount_{0};
    std::unordered_map<std::string_view, std::pair<std::string, bool>>
        preparedStatementsMap_;
#else
    std::unordered_map<std::string_view, std::string> preparedStatementsMap_;
#endif

    MessageCallback messageCallback_;
};

}  // namespace orm
}  // namespace drogon
