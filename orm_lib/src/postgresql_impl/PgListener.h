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

namespace drogon
{
namespace orm
{

class PgListener : public DbListener
{
    using MessageCallback = std::function<void(std::string, std::string)>;

  public:
    explicit PgListener(DbClientPtr dbClient);
    std::shared_ptr<DbClient> dbClient() const override;

    void listen(const std::string& channel,
                MessageCallback messageCallback) override;
    void unlisten(const std::string& channel) noexcept override;

    void onMessage(const std::string& channel,
                   const std::string& message,
                   trantor::EventLoop* loop) const;

  private:
    DbClientPtr dbClient_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<MessageCallback>>
        listenChannels_;
};

}  // namespace orm

}  // namespace drogon
