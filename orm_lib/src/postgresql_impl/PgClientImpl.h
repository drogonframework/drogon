#pragma once

#include "DbConnection.h"
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

  private:
    void ioLoop();
    std::unique_ptr<trantor::EventLoop> _loopPtr;
    enum ConnectStatus
    {
        ConnectStatus_None = 0,
        ConnectStatus_Connecting,
        ConnectStatus_Ok,
        ConnectStatus_Bad
    };

    void execSql(const DbConnectionPtr &conn, const std::string &sql,
                 size_t paraNum,
                 const std::vector<const char *> &parameters,
                 const std::vector<int> &length,
                 const std::vector<int> &format,
                 const ResultCallback &rcb,
                 const std::function<void(const std::exception_ptr &)> &exceptCallback);

    DbConnectionPtr newConnection(trantor::EventLoop *loop);
    std::unordered_set<DbConnectionPtr> _connections;
    std::unordered_set<DbConnectionPtr> _readyConnections;
    std::unordered_set<DbConnectionPtr> _busyConnections;
    std::string _connInfo;
    std::thread _loopThread;
    std::mutex _connectionsMutex;
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
};
} // namespace orm

} // namespace drogon