//
// Created by wanchen.he on 2022/9/14.
//

#include "DbSubscribeContext.h"

using namespace drogon::orm;

std::atomic<unsigned long long> DbSubscribeContext::maxContextId_{0};

DbSubscribeContext::DbSubscribeContext(std::weak_ptr<DbSubscriber> weakSub,
                                       std::string channel)
    : contextId_(++maxContextId_),
      weakSub_(std::move(weakSub)),
      channel_(std::move(channel)),
      subscribeCommand_("LISTEN " + channel_),
      unsubscribeCommand_("UNLISTEN " + channel_)
{
}

void DbSubscribeContext::onMessage(const std::string& channel,
                                   const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& callback : messageCallbacks_)
    {
        callback(channel, message);
    }
}
