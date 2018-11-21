#pragma once

#include "../DbConnection.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/NonCopyable.h>
#include <libpq-fe.h>
#include <memory>
#include <string>
#include <functional>
#include <iostream>

namespace drogon
{
namespace orm
{

class PgConnection;
typedef std::shared_ptr<PgConnection> PgConnectionPtr;
class PgConnection : public DbConnection, public std::enable_shared_from_this<PgConnection>
{
  public:
    PgConnection(trantor::EventLoop *loop, const std::string &connInfo);

    void execSql(const std::string &sql,
                 size_t paraNum,
                 const std::vector<const char *> &parameters,
                 const std::vector<int> &length,
                 const std::vector<int> &format,
                 const ResultCallback &rcb,
                 const std::function<void(const std::exception_ptr &)> &exceptCallback,
                 const std::function<void()> &idleCb);
  private:
    std::shared_ptr<PGconn> _connPtr;
    trantor::Channel _channel;
    void handleRead();
    void pgPoll();
    void handleClosed();
};

} // namespace orm
} // namespace drogon
