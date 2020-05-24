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
using SubscriberID = uint64_t;

namespace inner
{
/**
 * @brief this class is a helper class, don't use it directly.
 *
 * @tparam MessageType
 */
template <typename MessageType>
class Topic : public trantor::NonCopyable
{
  public:
    using MessageHandler =
        std::function<void(const std::string &, const MessageType &)>;
#if __cplusplus >= 201703L | defined _WIN32
    using SharedMutex = std::shared_mutex;
#else
    using SharedMutex = std::shared_timed_mutex;
#endif
    Topic(const std::string &topic) : topic_(topic)
    {
    }
    void publish(const MessageType &message) const
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        for (auto &pair : handlersMap_)
        {
            pair.second(topic_, message);
        }
    }
    SubscriberID subscribe(const MessageHandler &handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_[++id_] = handler;
        return id_;
    }
    SubscriberID subscribe(MessageHandler &&handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_[++id_] = std::move(handler);
        return id_;
    }
    void unsubscribe(SubscriberID id)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_.erase(id);
    }
    bool empty()
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        return handlersMap_.empty();
    }

  private:
    std::unordered_map<SubscriberID, MessageHandler> handlersMap_;
    const std::string topic_;
    mutable SharedMutex mutex_;
    SubscriberID id_;
};
}  // namespace inner
/**
 * @brief This class template implements a publish-subscribe pattern.
 *
 * @tparam MessageType The message type.
 */
template <typename MessageType>
class PubSubService : public trantor::NonCopyable
{
  public:
    using MessageHandler =
        std::function<void(const std::string &, const MessageType &)>;
#if __cplusplus >= 201703L | defined _WIN32
    using SharedMutex = std::shared_mutex;
#else
    using SharedMutex = std::shared_timed_mutex;
#endif
    /**
     * @brief Publish a message to a topic. The message will be broadcasted to
     * every subscriber.
     */
    void publish(const std::string &topic, const MessageType &message) const
    {
        std::shared_ptr<inner::Topic<MessageType>> topicPtr;
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topic);
            if (iter != topicMap_.end())
            {
                topicPtr = iter->second;
            }
            else
            {
                return;
            }
        }
        topicPtr->publish(message);
    }
    /**
     * @brief Subscribe a topic. When a message is published to the topic, the
     * handler is invoked by passing the topic and message as parameters.
     */
    SubscriberID subscribe(const std::string &topic,
                           const MessageHandler &handler)
    {
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topic);
            if (iter != topicMap_.end())
            {
                return iter->second->subscribe(handler);
            }
        }
        std::unique_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topic);
        if (iter != topicMap_.end())
        {
            return iter->second->subscribe(handler);
        }
        auto topicPtr = std::make_shared<inner::Topic<MessageType>>(topic);
        auto id = topicPtr->subscribe(handler);
        topicMap_[topic] = std::move(topicPtr);
        return id;
    }
    /**
     * @brief Subscribe a topic. When a message is published to the topic, the
     * handler is invoked by passing the topic and message as parameters.
     */
    SubscriberID subscribe(const std::string &topic, MessageHandler &&handler)
    {
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topic);
            if (iter != topicMap_.end())
            {
                return iter->second->subscribe(std::move(handler));
            }
        }
        std::unique_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topic);
        if (iter != topicMap_.end())
        {
            return iter->second->subscribe(std::move(handler));
        }
        auto topicPtr = std::make_shared<inner::Topic<MessageType>>(topic);
        auto id = topicPtr->subscribe(std::move(handler));
        topicMap_[topic] = std::move(topicPtr);
        return id;
    }

    /**
     * @brief Unsubscribe from a topic.
     *
     * @param topic
     * @param id The subscriber ID returned from the subscribe method.
     */
    void unsubscribe(const std::string &topic, SubscriberID id)
    {
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topic);
            if (iter == topicMap_.end())
            {
                return;
            }
            iter->second->unsubscribe(id);
            if (!iter->second->empty())
                return;
        }
        std::unique_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topic);
        if (iter == topicMap_.end())
        {
            return;
        }
        if (iter->second->empty())
            topicMap_.erase(iter);
    }
    /**
     * @brief return the number of topics.
     */
    size_t size() const
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        return topicMap_.size();
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<inner::Topic<MessageType>>>
        topicMap_;
    mutable SharedMutex mutex_;
    SubscriberID subID_ = 0;
};
}  // namespace drogon
