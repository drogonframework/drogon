/**
 *
 *  @file PubSubService.h
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

#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <memory>
#include <unordered_map>

namespace drogon
{
using SubscriberID = uint64_t;

/**
 * @brief This class template presents an unnamed topic.
 *
 * @tparam MessageType
 */
template <typename MessageType>
class Topic : public trantor::NonCopyable
{
  public:
    using MessageHandler = std::function<void(const MessageType &)>;
#if __cplusplus >= 201703L | defined _WIN32
    using SharedMutex = std::shared_mutex;
#else
    using SharedMutex = std::shared_timed_mutex;
#endif
    /**
     * @brief Publish a message, every subscriber in the topic will receive the
     * message.
     *
     * @param message
     */
    void publish(const MessageType &message) const
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        for (auto &pair : handlersMap_)
        {
            pair.second(message);
        }
    }

    /**
     * @brief Subscribe to the topic.
     *
     * @param handler is invoked when a message arrives.
     * @return SubscriberID
     */
    SubscriberID subscribe(const MessageHandler &handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_[++id_] = handler;
        return id_;
    }

    /**
     * @brief Subscribe to the topic.
     *
     * @param handler is invoked when a message arrives.
     * @return SubscriberID
     */
    SubscriberID subscribe(MessageHandler &&handler)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_[++id_] = std::move(handler);
        return id_;
    }

    /**
     * @brief Unsubscribe from the topic.
     */
    void unsubscribe(SubscriberID id)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_.erase(id);
    }

    /**
     * @brief Check if the topic is empty.
     *
     * @return true means there are no subscribers.
     * @return false means there are subscribers in the topic.
     */
    bool empty() const
    {
        std::shared_lock<SharedMutex> lock(mutex_);
        return handlersMap_.empty();
    }

    /**
     * @brief Remove all subscribers from the topic.
     *
     */
    void clear()
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        handlersMap_.clear();
    }

  private:
    std::unordered_map<SubscriberID, MessageHandler> handlersMap_;
    mutable SharedMutex mutex_;
    SubscriberID id_{0};
};

/**
 * @brief This class template implements a publish-subscribe pattern with
 * multiple named topics.
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
    void publish(const std::string &topicName, const MessageType &message) const
    {
        std::shared_ptr<Topic<MessageType>> topicPtr;
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topicName);
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
     * @brief Subscribe to a topic. When a message is published to the topic,
     * the handler is invoked by passing the topic and message as parameters.
     */
    SubscriberID subscribe(const std::string &topicName,
                           const MessageHandler &handler)
    {
        auto topicHandler = [topicName, handler](const MessageType &message) {
            handler(topicName, message);
        };
        return subscribeToTopic(topicName, std::move(topicHandler));
    }

    /**
     * @brief Subscribe to a topic. When a message is published to the topic,
     * the handler is invoked by passing the topic and message as parameters.
     * @param topicName Topic name.
     * @param handler The message handler.
     * @return The subscriber ID.
     */
    SubscriberID subscribe(const std::string &topicName,
                           MessageHandler &&handler)
    {
        auto topicHandler = [topicName, handler = std::move(handler)](
                                const MessageType &message) {
            handler(topicName, message);
        };
        return subscribeToTopic(topicName, std::move(topicHandler));
    }

    /**
     * @brief Unsubscribe from a topic.
     *
     * @param topicName Topic name.
     * @param id The subscriber ID returned from the subscribe method.
     */
    void unsubscribe(const std::string &topicName, SubscriberID id)
    {
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topicName);
            if (iter == topicMap_.end())
            {
                return;
            }
            iter->second->unsubscribe(id);
            if (!iter->second->empty())
                return;
        }
        std::unique_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topicName);
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

    /**
     * @brief remove all topics.
     */
    void clear()
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        topicMap_.clear();
    }

    /**
     * @brief Remove a topic
     *
     */
    void removeTopic(const std::string &topicName)
    {
        std::unique_lock<SharedMutex> lock(mutex_);
        topicMap_.erase(topicName);
    }

    /**
     * @brief Check if a topic is empty.
     *
     * @param topicName The topic name.
     * @return true means there are no subscribers.
     * @return false means there are subscribers in the topic.
     */
    bool isTopicEmpty(const std::string &topicName) const
    {
        std::shared_ptr<Topic<MessageType>> topicPtr;
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topicName);
            if (iter != topicMap_.end())
            {
                topicPtr = iter->second;
            }
            else
            {
                return true;
            }
        }
        return topicPtr->empty();
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<Topic<MessageType>>>
        topicMap_;
    mutable SharedMutex mutex_;
    SubscriberID subID_ = 0;

    SubscriberID subscribeToTopic(
        const std::string &topicName,
        typename Topic<MessageType>::MessageHandler &&handler)
    {
        {
            std::shared_lock<SharedMutex> lock(mutex_);
            auto iter = topicMap_.find(topicName);
            if (iter != topicMap_.end())
            {
                return iter->second->subscribe(std::move(handler));
            }
        }
        std::unique_lock<SharedMutex> lock(mutex_);
        auto iter = topicMap_.find(topicName);
        if (iter != topicMap_.end())
        {
            return iter->second->subscribe(std::move(handler));
        }
        auto topicPtr = std::make_shared<Topic<MessageType>>();
        auto id = topicPtr->subscribe(std::move(handler));
        topicMap_[topicName] = std::move(topicPtr);
        return id;
    }
};
}  // namespace drogon
