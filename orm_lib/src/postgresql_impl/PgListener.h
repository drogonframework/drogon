/**
 *
 *  @file PgListener.h
 *  @author Nitromelon
 *
 *  Copyright 2022, An Tao.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/orm/DbListener.h>
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThread.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include "../DbListenerMixin.h"

namespace drogon
{
namespace orm
{
class PgListener : public DbListener,
                   public DbListenerMixin,
                   public std::enable_shared_from_this<PgListener>
{
  public:
    explicit PgListener(trantor::EventLoop* loop);
    ~PgListener() override;

    void setDbClient(DbClientPtr dbClient);
    trantor::EventLoop* getLoop() const
    {
        return loop_;
    }

    void listen(const std::string& channel,
                MessageCallback messageCallback) noexcept override;
    void unlisten(const std::string& channel) noexcept override;

    void onMessage(const std::string& channel,
                   const std::string& message) const noexcept override;
    void listenAll() noexcept override;

  private:
    static std::string formatListenCommand(const std::string& channel);
    static std::string formatUnlistenCommand(const std::string& channel);

    void doListen(const std::string& channel);
    void doListenInLoop(const std::string& channel);

    std::unique_ptr<trantor::EventLoopThread> threadPtr_;
    trantor::EventLoop* loop_;
    DbClientPtr dbClient_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<MessageCallback>>
        listenChannels_;
};

}  // namespace orm
}  // namespace drogon
