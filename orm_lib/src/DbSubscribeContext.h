//
// Created by wanchen.he on 2022/9/14.
//

#pragma once

#include <drogon/orm/SqlBinder.h>
#include <list>
#include <atomic>

namespace drogon
{
namespace orm
{
class DbSubscriber;

class DbSubscribeContext : trantor::NonCopyable
{
  public:
    using MessageCallback = std::function<void(const std::string& channel,
                                               const std::string& payload)>;

    DbSubscribeContext(std::weak_ptr<DbSubscriber> weakSub,
                       std::string channel);

    unsigned long long contextId() const
    {
        return contextId_;
    }

    const std::string& channel() const
    {
        return channel_;
    }

    const std::string& subscribeCommand() const
    {
        return subscribeCommand_;
    }

    const std::string& unsubscribeCommand() const
    {
        return unsubscribeCommand_;
    }

    void addMessageCallback(MessageCallback&& messageCallback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.emplace_back(std::move(messageCallback));
    }

    void onMessage(const std::string& channel, const std::string& message);

    void disable()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_ = true;
        messageCallbacks_.clear();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.clear();
    }

    bool alive() const
    {
        return !disabled_ && weakSub_.lock() != nullptr;
    }

  protected:
    static std::atomic<unsigned long long> maxContextId_;

    unsigned long long contextId_;
    std::weak_ptr<DbSubscriber> weakSub_;
    std::string channel_;
    std::string subscribeCommand_;
    std::string unsubscribeCommand_;
    std::mutex mutex_;
    std::list<MessageCallback> messageCallbacks_;
    bool disabled_{false};
};

}  // namespace orm
}  // namespace drogon
