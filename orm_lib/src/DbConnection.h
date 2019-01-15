/**
 *
 *  DbConnection.h
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

#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/NonCopyable.h>
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

class DbConnection;
typedef std::shared_ptr<DbConnection> DbConnectionPtr;
class DbConnection : public trantor::NonCopyable
{
  public:
    typedef std::function<void(const DbConnectionPtr &)> DbConnectionCallback;
    DbConnection(trantor::EventLoop *loop) : _loop(loop) {}
    void setOkCallback(const DbConnectionCallback &cb)
    {
        _okCb = cb;
    }
    void setCloseCallback(const DbConnectionCallback &cb)
    {
        _closeCb = cb;
    }
    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback,
                         std::function<void()> &&idleCb) = 0;
    virtual ~DbConnection()
    {
        LOG_TRACE << "Destruct DbConn" << this;
    }
    ConnectStatus status() const { return _status; }
    trantor::EventLoop *loop() { return _loop; }
    virtual void disconnect() = 0;

  protected:
    QueryCallback _cb;
    trantor::EventLoop *_loop;
    std::function<void()> _idleCb;
    ConnectStatus _status = ConnectStatus_None;
    DbConnectionCallback _closeCb = [](const DbConnectionPtr &) {};
    DbConnectionCallback _okCb = [](const DbConnectionPtr &) {};
    std::function<void(const std::exception_ptr &)> _exceptCb;
    bool _isWorking = false;
    std::string _sql = "";
};

} // namespace orm
} // namespace drogon
