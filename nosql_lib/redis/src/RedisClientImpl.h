/**
 *
 *  @file RedisClientImpl.h
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

#include "RedisConnection.h"
#include <drogon/nosql/RedisClient.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <set>

namespace drogon
{
namespace nosql
{
class RedisClientImpl : public RedisClient, public trantor::NonCopyable
{
  public:
    RedisClientImpl(const trantor::InetAddress &serverAddress,
                    const size_t connectionNumber,
                    const std::string &password = "");
    virtual void execCommandAsync(
        const std::string &command,
        std::function<void(const RedisResult &)> &&commandCallback,
        std::function<void(const std::exception &)>
        &&exceptCallback) noexcept override;
  private:
    trantor::EventLoopThreadPool loops_;
    std::mutex connectionsMutex_;
    std::set<std::shared_ptr<RedisConnection>> connections_;
    std::shared_ptr<RedisConnection> newConnection(trantor::EventLoop *loop);
    const trantor::InetAddress serverAddr_;
    const std::string password_;
    const size_t connectionsNumber_;
};
}  // namespace nosql
}  // namespace drogon