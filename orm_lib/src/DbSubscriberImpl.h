//
// Created by wanchen.he on 2022/9/14.
//

#pragma once

#include <list>
#include <mutex>
#include <unordered_map>
#include <drogon/orm/DbSubscriber.h>
#include "DbSubscribeContext.h"

namespace drogon
{
namespace orm
{
class DbConnection;
using DbConnectionPtr = std::shared_ptr<DbConnection>;

class DbSubscriberImpl : public DbSubscriber,
                         public std::enable_shared_from_this<DbSubscriberImpl>
{
  public:
    ~DbSubscriberImpl() override;

    void subscribe(const std::string &channel,
                   MessageCallback &&messageCallback) noexcept override;
    void unsubscribe(const std::string &channel) noexcept override;

    // Set a connected connection to subscriber.
    void setConnection(const DbConnectionPtr &conn);
    // Clear connection and task queue.
    void clearConnection();
    // Subscribe next channel in task queue.
    void subscribeNext();
    // Subscribe all channels.
    void subscribeAll();

  private:
    DbConnectionPtr conn_;
    std::unordered_map<std::string, std::shared_ptr<DbSubscribeContext>>
        subContexts_;
    std::list<std::shared_ptr<std::function<void(const DbConnectionPtr &)>>>
        tasks_;
    std::mutex mutex_;
};

}  // namespace orm
}  // namespace drogon
