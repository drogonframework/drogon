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

    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)> &&exceptCallback,
                         std::function<void()> &&idleCb) override;

  private:
    std::shared_ptr<PGconn> _connPtr;
    trantor::Channel _channel;
    void handleRead();
    void pgPoll();
    void handleClosed();
};

} // namespace orm
} // namespace drogon
