/**
 *
 *  @file DbConnection.h
 *  @author An Tao
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

#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/string_view.h>
#include <trantor/net/EventLoop.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace drogon
{
namespace orm
{
#if __cplusplus >= 201703L | defined _WIN32
using SharedMutex = std::shared_mutex;
#else
using SharedMutex = std::shared_timed_mutex;
#endif

enum class ConnectStatus
{
    None = 0,
    Connecting,
    SettingCharacterSet,
    Ok,
    Bad
};

struct SqlCmd
{
    string_view sql_;
    size_t parametersNumber_;
    std::vector<const char *> parameters_;
    std::vector<int> lengths_;
    std::vector<int> formats_;
    QueryCallback callback_;
    ExceptPtrCallback exceptionCallback_;
    std::string preparingStatement_;
#if LIBPQ_SUPPORTS_BATCH_MODE
    bool isChanging_;
#endif
    SqlCmd(string_view &&sql,
           const size_t paraNum,
           std::vector<const char *> &&parameters,
           std::vector<int> &&length,
           std::vector<int> &&format,
           QueryCallback &&cb,
           ExceptPtrCallback &&exceptCb)
        : sql_(std::move(sql)),
          parametersNumber_(paraNum),
          parameters_(std::move(parameters)),
          lengths_(std::move(length)),
          formats_(std::move(format)),
          callback_(std::move(cb)),
          exceptionCallback_(std::move(exceptCb))
    {
    }
};

class DbConnection;
using DbConnectionPtr = std::shared_ptr<DbConnection>;
class DbConnection : public trantor::NonCopyable
{
  public:
    using DbConnectionCallback = std::function<void(const DbConnectionPtr &)>;
    DbConnection(trantor::EventLoop *loop) : loop_(loop)
    {
    }
    void setOkCallback(const DbConnectionCallback &cb)
    {
        okCallback_ = cb;
    }
    void setCloseCallback(const DbConnectionCallback &cb)
    {
        closeCallback_ = cb;
    }
    void setIdleCallback(const std::function<void()> &cb)
    {
        idleCb_ = cb;
    }
    virtual void execSql(
        string_view &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback) = 0;
    virtual void batchSql(
        std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) = 0;
    virtual ~DbConnection()
    {
        LOG_TRACE << "Destruct DbConn" << this;
    }
    ConnectStatus status() const
    {
        return status_;
    }
    trantor::EventLoop *loop()
    {
        return loop_;
    }
    virtual void disconnect() = 0;
    bool isWorking()
    {
        return isWorking_;
    }

  protected:
    QueryCallback callback_;
    trantor::EventLoop *loop_;
    std::function<void()> idleCb_;
    ConnectStatus status_{ConnectStatus::None};
    DbConnectionCallback closeCallback_{[](const DbConnectionPtr &) {}};
    DbConnectionCallback okCallback_{[](const DbConnectionPtr &) {}};
    std::function<void(const std::exception_ptr &)> exceptionCallback_;
    bool isWorking_{false};

    static std::map<std::string, std::string> parseConnString(
        const std::string &);
};

}  // namespace orm
}  // namespace drogon
