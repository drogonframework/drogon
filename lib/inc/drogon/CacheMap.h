/**
 *
 *  CacheMap.h
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

#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <future>
#include <assert.h>

#define WHEELS_NUM 4
#define BUCKET_NUM_PER_WHEEL 200
#define TICK_INTERVAL 1.0

namespace drogon
{
/**
 * @brief A utility class for CacheMap
 */
class CallbackEntry
{
  public:
    CallbackEntry(std::function<void()> cb) : cb_(std::move(cb))
    {
    }
    ~CallbackEntry()
    {
        cb_();
    }

  private:
    std::function<void()> cb_;
};

using CallbackEntryPtr = std::shared_ptr<CallbackEntry>;
using WeakCallbackEntryPtr = std::weak_ptr<CallbackEntry>;

using CallbackBucket = std::unordered_set<CallbackEntryPtr>;
using CallbackBucketQueue = std::deque<CallbackBucket>;

/**
 * @brief Cache Map
 *
 * @tparam T1 The keyword type.
 * @tparam T2 The value type.
 * @note
 * Four wheels with 200 buckets per wheel means the cache map can work with a
 * timeout up to 200^4 seconds (about 50 years).
 */
template <typename T1, typename T2>
class CacheMap
{
  public:
    /// constructor
    /**
     * @param loop
     * eventloop pointer
     * @param tickInterval
     * second
     * @param wheelsNum
     * number of wheels
     * @param bucketsNumPerWheel
     * buckets number per wheel
     * The max delay of the CacheMap is about
     * tickInterval*(bucketsNumPerWheel^wheelsNum) seconds.
     */
    CacheMap(trantor::EventLoop *loop,
             float tickInterval = TICK_INTERVAL,
             size_t wheelsNum = WHEELS_NUM,
             size_t bucketsNumPerWheel = BUCKET_NUM_PER_WHEEL)
        : loop_(loop),
          tickInterval_(tickInterval),
          wheelsNumber_(wheelsNum),
          bucketsNumPerWheel_(bucketsNumPerWheel)
    {
        wheels_.resize(wheelsNumber_);
        for (size_t i = 0; i < wheelsNumber_; ++i)
        {
            wheels_[i].resize(bucketsNumPerWheel_);
        }
        if (tickInterval_ > 0 && wheelsNumber_ > 0 && bucketsNumPerWheel_ > 0)
        {
            timerId_ = loop_->runEvery(tickInterval_, [=]() {
                size_t t = ++ticksCounter_;
                size_t pow = 1;
                for (size_t i = 0; i < wheelsNumber_; ++i)
                {
                    if ((t % pow) == 0)
                    {
                        CallbackBucket tmp;
                        {
                            std::lock_guard<std::mutex> lock(bucketMutex_);
                            // use tmp val to make this critical area as short
                            // as possible.
                            wheels_[i].front().swap(tmp);
                            wheels_[i].pop_front();
                            wheels_[i].push_back(CallbackBucket());
                        }
                    }
                    pow = pow * bucketsNumPerWheel_;
                }
            });
        }
        else
        {
            noWheels_ = true;
        }
    };
    ~CacheMap()
    {
        {
            std::lock_guard<std::mutex> guard(mtx_);
            map_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(bucketMutex_);
            for (auto iter = wheels_.rbegin(); iter != wheels_.rend(); ++iter)
            {
                iter->clear();
            }
        }
        LOG_TRACE << "CacheMap destruct!";
    }
    struct MapValue
    {
        size_t timeout = 0;
        T2 value;
        std::function<void()> timeoutCallback_;
        WeakCallbackEntryPtr weakEntryPtr_;
    };

    /**
     * @brief Insert a key-value pair into the cache.
     *
     * @param key The key
     * @param value The value
     * @param timeout The timeout in seconds, if timeout > 0, the value will be
     * erased within the 'timeout' seconds after the last access. If the timeout
     * is zero, the value exists until being removed explicitly.
     * @param timeoutCallback is called when the timeout expires.
     */
    void insert(const T1 &key,
                T2 &&value,
                size_t timeout = 0,
                std::function<void()> timeoutCallback = std::function<void()>())
    {
        if (timeout > 0)
        {
            MapValue v;
            v.value = std::move(value);
            v.timeout = timeout;
            v.timeoutCallback_ = std::move(timeoutCallback);
            std::lock_guard<std::mutex> lock(mtx_);
            map_[key] = std::move(v);
            eraseAfter(timeout, key);
        }
        else
        {
            MapValue v;
            v.value = std::move(value);
            v.timeout = timeout;
            v.timeoutCallback_ = std::function<void()>();
            v.weakEntryPtr_ = WeakCallbackEntryPtr();
            std::lock_guard<std::mutex> lock(mtx_);
            map_[key] = std::move(v);
        }
    }
    /**
     * @brief Insert a key-value pair into the cache.
     *
     * @param key The key
     * @param value The value
     * @param timeout The timeout in seconds, if timeout > 0, the value will be
     * erased within the 'timeout' seconds after the last access. If the timeout
     * is zero, the value exists until being removed explicitly.
     * @param timeoutCallback is called when the timeout expires.
     */
    void insert(const T1 &key,
                const T2 &value,
                size_t timeout = 0,
                std::function<void()> timeoutCallback = std::function<void()>())
    {
        if (timeout > 0)
        {
            MapValue v;
            v.value = value;
            v.timeout = timeout;
            v.timeoutCallback_ = std::move(timeoutCallback);
            std::lock_guard<std::mutex> lock(mtx_);
            map_[key] = std::move(v);
            eraseAfter(timeout, key);
        }
        else
        {
            MapValue v;
            v.value = value;
            v.timeout = timeout;
            v.timeoutCallback_ = std::function<void()>();
            v.weakEntryPtr_ = WeakCallbackEntryPtr();
            std::lock_guard<std::mutex> lock(mtx_);
            map_[key] = std::move(v);
        }
    }

    /// Return the reference to the value of the keyword.
    T2 &operator[](const T1 &key)
    {
        int timeout = 0;

        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = map_.find(key);
        if (iter != map_.end())
        {
            timeout = iter->second.timeout;
            if (timeout > 0)
                eraseAfter(timeout, key);
            return iter->second.value;
        }
        return map_[key].value;
    }

    /// Check if the value of the keyword exists
    bool find(const T1 &key)
    {
        int timeout = 0;
        bool flag = false;

        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = map_.find(key);
        if (iter != map_.end())
        {
            timeout = iter->second.timeout;
            flag = true;
        }

        if (timeout > 0)
            eraseAfter(timeout, key);

        return flag;
    }

    /// Atomically find and get the value of a keyword
    /**
     * Return true when the value is found, and the value
     * is assigned to the value argument.
     */
    bool findAndFetch(const T1 &key, T2 &value)
    {
        size_t timeout = 0;
        bool flag = false;
        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = map_.find(key);
        if (iter != map_.end())
        {
            timeout = iter->second.timeout;
            flag = true;
            value = iter->second.value;
        }

        if (timeout > 0)
            eraseAfter(timeout, key);

        return flag;
    }

    /// Erase the value of the keyword.
    /**
     * @param key the keyword.
     * @note This function does not cause the timeout callback to be executed.
     */
    void erase(const T1 &key)
    {
        // in this case,we don't evoke the timeout callback;
        std::lock_guard<std::mutex> lock(mtx_);
        map_.erase(key);
    }
    /**
     * @brief Get the event loop object
     *
     * @return trantor::EventLoop*
     */
    trantor::EventLoop *getLoop()
    {
        return loop_;
    }

  private:
    std::unordered_map<T1, MapValue> map_;

    std::vector<CallbackBucketQueue> wheels_;

    std::atomic<size_t> ticksCounter_{0};

    std::mutex mtx_;
    std::mutex bucketMutex_;
    trantor::TimerId timerId_;
    trantor::EventLoop *loop_;

    float tickInterval_;
    size_t wheelsNumber_;
    size_t bucketsNumPerWheel_;

    bool noWheels_{false};

    void insertEntry(size_t delay, CallbackEntryPtr entryPtr)
    {
        // protected by bucketMutex;
        if (delay <= 0)
            return;
        delay = static_cast<size_t>(delay / tickInterval_ + 1);
        size_t t = ticksCounter_;
        for (size_t i = 0; i < wheelsNumber_; ++i)
        {
            if (delay <= bucketsNumPerWheel_)
            {
                wheels_[i][delay - 1].insert(entryPtr);
                break;
            }
            if (i < (wheelsNumber_ - 1))
            {
                entryPtr = std::make_shared<CallbackEntry>([=]() {
                    if (delay > 0)
                    {
                        std::lock_guard<std::mutex> lock(bucketMutex_);
                        wheels_[i][(delay + (t % bucketsNumPerWheel_) - 1) %
                                   bucketsNumPerWheel_]
                            .insert(entryPtr);
                    }
                });
            }
            else
            {
                // delay is too long to put entry at valid position in wheels;
                wheels_[i][bucketsNumPerWheel_ - 1].insert(entryPtr);
            }
            delay =
                (delay + (t % bucketsNumPerWheel_) - 1) / bucketsNumPerWheel_;
            t = t / bucketsNumPerWheel_;
        }
    }
    void eraseAfter(size_t delay, const T1 &key)
    {
        if (noWheels_)
            return;
        assert(map_.find(key) != map_.end());

        CallbackEntryPtr entryPtr;

        if (map_.find(key) != map_.end())
        {
            entryPtr = map_[key].weakEntryPtr_.lock();
        }

        if (entryPtr)
        {
            std::lock_guard<std::mutex> lock(bucketMutex_);
            insertEntry(delay, entryPtr);
        }
        else
        {
            std::function<void()> cb = [=]() {
                std::lock_guard<std::mutex> lock(mtx_);
                if (map_.find(key) != map_.end())
                {
                    auto &value = map_[key];
                    auto entryPtr = value.weakEntryPtr_.lock();
                    // entryPtr is used to avoid race conditions
                    if (value.timeout > 0 && !entryPtr)
                    {
                        if (value.timeoutCallback_)
                        {
                            value.timeoutCallback_();
                        }
                        map_.erase(key);
                    }
                }
            };

            entryPtr = std::make_shared<CallbackEntry>(cb);
            map_[key].weakEntryPtr_ = entryPtr;

            {
                std::lock_guard<std::mutex> lock(bucketMutex_);
                insertEntry(delay, entryPtr);
            }
        }
    }
};

}  // namespace drogon
