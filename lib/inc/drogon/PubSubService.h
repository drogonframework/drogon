/**
 *
 *  PubSubService.h
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

#include <trantor/utils/NonCopyable.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <shared_mutex>

namespace drogon
{
/**
 * @brief This class template implements a publish-subscribe pattern.
 *
 * @tparam MessageType The message type.
 */
template <typename MessageType>
class PubSubService : public trantor::NonCopyable
{
  public:
#if __cplusplus >= 201703L | defined _WIN32
    using SharedMutex = std::shared_mutex;
#else
    using SharedMutex = std::shared_timed_mutex;
#endif
    using MessageHandler =
        std::function<void(const std::string &, const MessageType &)>;
    using SubscriberID = uint64_t;
    /**
     * @brief Publish a message to a topic. The message will be broadcasted to
     * every subscriber.
     */
    void publish(const std::string &topic, const MessageType &message)
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topic);
        if (iter != topicMap_.end())
        {
            for (auto &pair : iter->second)
            {
                pair.second(topic, message);
            }
        }
    }
    /**
     * @brief Subscribe a topic. When a message is published to the topic, the
     * handler is invoked by passing the topic and message as parameters.
     */
    SubscriberID subscribe(const std::string &topic,
                           const MessageHandler &handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        topicMap_[topic].insert(
            std::make_pair<SubscriberID, MessageHandler>(++subID_, handler));
        return subID_;
    }
    /**
     * @brief Subscribe a topic. When a message is published to the topic, the
     * handler is invoked by passing the topic and message as parameters.
     */
    SubscriberID subscribe(const std::string &topic, MessageHandler &&handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        topicMap_[topic].insert(
            std::make_pair<SubscriberID, MessageHandler>(++subID_,
                                                         std::move(handler)));
        return subID_;
    }
    /**
     * @brief Unsubscribe from a topic.
     *
     * @param topic
     * @param id The subscriber ID returned from the subscribe method.
     */
    void unsubscribe(const std::string &topic, SubscriberID id)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        topicMap_[topic].erase(id);
    }

  private:
    std::unordered_map<std::string,
                       std::unordered_map<SubscriberID, MessageHandler>>
        topicMap_;
    SharedMutex mutex_;
    SubscriberID subID_ = 0;
};
}  // namespace drogon
