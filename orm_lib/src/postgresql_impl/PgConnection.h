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
#include <libpq-fe.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <iostream>
#include <list>
#include <set>

namespace drogon
{
namespace orm
{
class PgConnection;
using PgConnectionPtr = std::shared_ptr<PgConnection>;

// Upper bound on the number of prepared statements cached per physical
// connection. Without a bound, workloads that produce many distinct SQL texts
// (for example multi-row INSERTs whose row count varies between calls) grow
// both the client-side map and the server-side CachedPlan contexts until the
// backend exhausts memory. PostgreSQL only frees them when the connection
// closes, so long-lived pooled connections leak. Zero disables the limit.
static constexpr size_t maxPreparedStatementsCount = 256;

struct PreparedStatementInfo
{
    std::string name;
    bool isChanging{false};
    std::list<std::string_view>::iterator lruIt;
    std::set<std::string>::iterator setIt;
};

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
    void handleFatalError();
    std::set<std::string> preparedStatements_;
    // Cache of prepared statements, keyed by SQL text. Keys are string_views
    // into the nodes owned by preparedStatements_ so each SQL string is stored
    // once. preparedStatementsLru_ tracks usage order; its front is the most
    // recently used and its back the least recently used entry to evict.
    std::unordered_map<std::string_view, PreparedStatementInfo>
        preparedStatementsMap_;
    std::list<std::string_view> preparedStatementsLru_;
    size_t maxPreparedStatements_{maxPreparedStatementsCount};
    std::string_view sql_;
#if LIBPQ_SUPPORTS_BATCH_MODE
    void handleFatalError(bool clearAll, bool isAbortPipeline = false);
    void sendMaintenanceDeallocate(const std::string &name);
    std::list<std::shared_ptr<SqlCmd>> batchCommandsForWaitingResults_;
    std::deque<std::shared_ptr<SqlCmd>> batchSqlCommands_;
    void sendBatchedSql();
    int sendBatchEnd();
    bool sendBatchEnd_{false};
    bool autoBatch_{false};
    unsigned int batchCount_{0};
#else
    // Names of server-side prepared statements to DEALLOCATE. Handled one at a
    // time once the connection becomes idle.
    std::deque<std::string> pendingDealloc_;
    bool isDeallocating_{false};
    bool startNextDeallocate();
#endif

    MessageCallback messageCallback_;

    // Insert a statement into the cache, evicting the least recently used
    // entry when the cache is at capacity. Returns the name of a server-side
    // statement that must be DEALLOCATED: the evicted entry, or `name` itself
    // when the SQL is already cached (a concurrent command in the same batch
    // prepared it first). Empty when nothing needs deallocating.
    std::string cachePreparedStatement(std::string_view sql,
                                       std::string name,
                                       bool isChanging)
    {
        auto it = preparedStatementsMap_.find(sql);
        if (it != preparedStatementsMap_.end())
        {
            touchPreparedStatement(it->second);
            return name;
        }
        std::string toDeallocate;
        if (maxPreparedStatements_ != 0 &&
            preparedStatementsMap_.size() >= maxPreparedStatements_)
        {
            toDeallocate = evictLruPreparedStatement();
        }
        auto r = preparedStatements_.insert(std::string{sql});
        auto lruIt =
            preparedStatementsLru_.insert(preparedStatementsLru_.begin(),
                                          std::string_view{r.first->data(),
                                                           r.first->length()});
        preparedStatementsMap_[std::string_view{r.first->data(),
                                                r.first->length()}] =
            PreparedStatementInfo{std::move(name), isChanging, lruIt, r.first};
        return toDeallocate;
    }

    std::string evictLruPreparedStatement()
    {
        if (preparedStatementsLru_.empty())
            return {};
        auto key = preparedStatementsLru_.back();
        auto it = preparedStatementsMap_.find(key);
        if (it == preparedStatementsMap_.end())
        {
            preparedStatementsLru_.pop_back();
            return {};
        }
        std::string name = std::move(it->second.name);
        preparedStatements_.erase(it->second.setIt);
        preparedStatementsMap_.erase(it);
        preparedStatementsLru_.pop_back();
        return name;
    }

    void touchPreparedStatement(PreparedStatementInfo &info)
    {
        preparedStatementsLru_.splice(preparedStatementsLru_.begin(),
                                      preparedStatementsLru_,
                                      info.lruIt);
    }
};

}  // namespace orm
}  // namespace drogon
