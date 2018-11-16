#pragma once

#include "PgConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoop.h>
#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <unordered_set>
#include <list>

namespace drogon
{
namespace orm
{
// extern Result makeResult(SqlStatus status, const std::shared_ptr<PGresult> &r = std::shared_ptr<PGresult>(nullptr),
//                          const std::string &query = "");

class PgClientImpl : public DbClient
{
  public:
    PgClientImpl(const std::string &connInfo, const size_t connNum);
    ~PgClientImpl();
    virtual void execSql(const std::string &sql,
                         size_t paraNum,
                         const std::vector<const char *> &parameters,
                         const std::vector<int> &length,
                         const std::vector<int> &format,
                         const ResultCallback &rcb,
                         const std::function<void(const std::exception_ptr &)> &exceptCallback) override;
    virtual std::string replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const override;
    virtual std::shared_ptr<Transaction> newTransaction() override;

  private:
    void ioLoop();
    std::unique_ptr<trantor::EventLoop> _loopPtr;
    void execSql(const PgConnectionPtr &conn, const std::string &sql,
                 size_t paraNum,
                 const std::vector<const char *> &parameters,
                 const std::vector<int> &length,
                 const std::vector<int> &format,
                 const ResultCallback &rcb,
                 const std::function<void(const std::exception_ptr &)> &exceptCallback);

    PgConnectionPtr newConnection(trantor::EventLoop *loop);
    std::unordered_set<PgConnectionPtr> _connections;
    std::unordered_set<PgConnectionPtr> _readyConnections;
    std::unordered_set<PgConnectionPtr> _busyConnections;
    std::string _connInfo;
    std::thread _loopThread;
    std::mutex _connectionsMutex;
    std::condition_variable _condConnectionReady;
    size_t _transWaitNum = 0;

    size_t _connectNum;
    bool _stop = false;

    struct SqlCmd
    {
        std::string _sql;
        size_t _paraNum;
        std::vector<std::string> _parameters;
        std::vector<int> _format;
        QueryCallback _cb;
        ExceptPtrCallback _exceptCb;
    };
    std::list<SqlCmd> _sqlCmdBuffer;
    std::mutex _bufferMutex;

    void handleNewTask(const PgConnectionPtr &conn);
};
} // namespace orm

} // namespace drogon