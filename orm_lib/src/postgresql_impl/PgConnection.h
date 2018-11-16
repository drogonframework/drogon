#pragma once

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

enum ConnectStatus
{
    ConnectStatus_None = 0,
    ConnectStatus_Connecting,
    ConnectStatus_Ok,
    ConnectStatus_Bad
};

class PgConnection;
typedef std::shared_ptr<PgConnection> PgConnectionPtr;
class PgConnection : public trantor::NonCopyable, public std::enable_shared_from_this<PgConnection>
{
  public:
    typedef std::function<void(const PgConnectionPtr &)> PgConnectionCallback;
    PgConnection(trantor::EventLoop *loop, const std::string &connInfo);

    void setOkCallback(const PgConnectionCallback &cb)
    {
        _okCb = cb;
    }
    void setCloseCallback(const PgConnectionCallback &cb)
    {
        _closeCb = cb;
    }
    void execSql(const std::string &sql,
                 size_t paraNum,
                 const std::vector<const char *> &parameters,
                 const std::vector<int> &length,
                 const std::vector<int> &format,
                 const ResultCallback &rcb,
                 const std::function<void(const std::exception_ptr &)> &exceptCallback,
                 const std::function<void()> &idleCb);
    ~PgConnection()
    {
        LOG_TRACE<<"Destruct DbConn"<<this;
    }
    int sock();
    trantor::EventLoop *loop() { return _loop; }
    ConnectStatus status() const { return _status; }

  private:
    std::shared_ptr<PGconn> _connPtr;
    trantor::EventLoop *_loop;
    trantor::Channel _channel;
    QueryCallback _cb;
    std::function<void()> _idleCb;
    ConnectStatus _status = ConnectStatus_None;
    PgConnectionCallback _closeCb = [](const PgConnectionPtr &) {};
    PgConnectionCallback _okCb = [](const PgConnectionPtr &) {};
    std::function<void(const std::exception_ptr &)> _exceptCb;
    bool _isWorking = false;
    std::string _sql = "";
    void handleRead();
    void pgPoll();
    void handleClosed();
};

} // namespace orm
} // namespace drogon
