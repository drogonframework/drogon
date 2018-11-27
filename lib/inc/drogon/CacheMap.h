/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <map>
#include <mutex>
#include <deque>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <assert.h>

#define WHEELS_NUM 4
#define BUCKET_NUM_PER_WHEEL 200
#define TICK_INTERVAL 1.0

//Four wheels with 200 buckets per wheel means the cache map can work with
//a timeout up to 200^4 seconds,about 50 years;

namespace drogon
{

class CallbackEntry
{
  public:
    CallbackEntry(std::function<void()> cb) : cb_(std::move(cb)) {}
    ~CallbackEntry()
    {
        cb_();
    }

  private:
    std::function<void()> cb_;
};

typedef std::shared_ptr<CallbackEntry> CallbackEntryPtr;
typedef std::weak_ptr<CallbackEntry> WeakCallbackEntryPtr;

typedef std::unordered_set<CallbackEntryPtr> CallbackBucket;
typedef std::deque<CallbackBucket> CallbackBucketQueue;

template <typename T1, typename T2>
class CacheMap
{
  public:
    /// constructor
    /// @param loop
    /// eventloop pointer
    /// @param tickInterval
    /// second
    /// @param wheelsNum
    /// number of wheels
    /// @param bucketsNumPerWheel
    /// buckets number per wheel
    /// The max delay of the CacheMap is about tickInterval*(bucketsNumPerWheel^wheelsNum) seconds.

    CacheMap(trantor::EventLoop *loop,
             float tickInterval = TICK_INTERVAL,
             size_t wheelsNum = WHEELS_NUM,
             size_t bucketsNumPerWheel = BUCKET_NUM_PER_WHEEL)
        : _loop(loop),
          _tickInterval(tickInterval),
          _wheelsNum(wheelsNum),
          _bucketsNumPerWheel(bucketsNumPerWheel)
    {
        _wheels.resize(_wheelsNum);
        for (size_t i = 0; i < _wheelsNum; i++)
        {
            _wheels[i].resize(_bucketsNumPerWheel);
        }
        if (_tickInterval > 0 &&
            _wheelsNum > 0 &&
            _bucketsNumPerWheel > 0)
        {
            _timerId = _loop->runEvery(_tickInterval, [=]() {
                _ticksCounter++;
                size_t t = _ticksCounter;
                size_t pow = 1;
                for (size_t i = 0; i < _wheelsNum; i++)
                {
                    if ((t % pow) == 0)
                    {
                        CallbackBucket tmp;
                        {
                            std::lock_guard<std::mutex> lock(bucketMutex_);
                            //use tmp val to make this critical area as short as possible.
                            _wheels[i].front().swap(tmp);
                            _wheels[i].pop_front();
                            _wheels[i].push_back(CallbackBucket());
                        }
                    }
                    pow = pow * _bucketsNumPerWheel;
                }
            });
        }
        else
        {
            _noWheels = true;
        }
    };
    ~CacheMap()
    {
        _loop->invalidateTimer(_timerId);
        {
            std::lock_guard<std::mutex> guard(mtx_);
            _map.clear();
        }

        for (int i = _wheels.size() - 1; i >= 0; i--)
        {
            _wheels[i].clear();
        }

        LOG_TRACE << "CacheMap destruct!";
    }
    typedef struct MapValue
    {
        size_t timeout = 0;
        T2 value;
        std::function<void()> _timeoutCallback;
        WeakCallbackEntryPtr _weakEntryPtr;
    } MapValue;

    //If timeout>0,the value will be erased
    //within the 'timeout' seconds after the last access

    void insert(const T1 &key, T2 &&value, size_t timeout = 0, std::function<void()> timeoutCallback = std::function<void()>())
    {
        if (timeout > 0)
        {
            MapValue v;
            v.value = std::move(value);
            v.timeout = timeout;
            v._timeoutCallback = std::move(timeoutCallback);
            std::lock_guard<std::mutex> lock(mtx_);
            _map[key] = std::move(v);
            eraseAfter(timeout, key);
        }
        else
        {
            MapValue v;
            v.value = std::move(value);
            v.timeout = timeout;
            v._timeoutCallback = std::function<void()>();
            v._weakEntryPtr = WeakCallbackEntryPtr();
            std::lock_guard<std::mutex> lock(mtx_);
            _map[key] = std::move(v);
        }
    }

    void insert(const T1 &key, const T2 &value, size_t timeout = 0, std::function<void()> timeoutCallback = std::function<void()>())
    {
        if (timeout > 0)
        {
            MapValue v;
            v.value = value;
            v.timeout = timeout;
            v._timeoutCallback = std::move(timeoutCallback);
            std::lock_guard<std::mutex> lock(mtx_);
            _map[key] = std::move(v);
            eraseAfter(timeout, key);
        }
        else
        {
            MapValue v;
            v.value = value;
            v.timeout = timeout;
            v._timeoutCallback = std::function<void()>();
            v._weakEntryPtr = WeakCallbackEntryPtr();
            std::lock_guard<std::mutex> lock(mtx_);
            _map[key] = std::move(v);
        }
    }

    T2 &operator[](const T1 &key)
    {
        int timeout = 0;

        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = _map.find(key);
        if (iter != _map.end())
        {
            timeout = iter->second.timeout;
            if (timeout > 0)
                eraseAfter(timeout, key);
            return iter->second.value;
        }
        return _map[key].value;
    }

    bool find(const T1 &key)
    {
        int timeout = 0;
        bool flag = false;

        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = _map.find(key);
        if (iter != _map.end())
        {
            timeout = iter->second.timeout;
            flag = true;
        }

        if (timeout > 0)
            eraseAfter(timeout, key);

        return flag;
    }

    bool findAndFetch(const T1 &key, T2 &value)
    {
        int timeout = 0;
        bool flag = false;
        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = _map.find(key);
        if (iter != _map.end())
        {
            timeout = iter->second.timeout;
            flag = true;
            value = iter->second.value;
        }

        if (timeout > 0)
            eraseAfter(timeout, key);

        return flag;
    }

    void erase(const T1 &key)
    {
        //in this case,we don't evoke the timeout callback;

        std::lock_guard<std::mutex> lock(mtx_);
        _map.erase(key);
    }

  private:
    std::unordered_map<T1, MapValue> _map;

    std::vector<CallbackBucketQueue> _wheels;

    std::atomic<size_t> _ticksCounter = ATOMIC_VAR_INIT(0);

    std::mutex mtx_;
    std::mutex bucketMutex_;
    trantor::TimerId _timerId;
    trantor::EventLoop *_loop;

    float _tickInterval;
    size_t _wheelsNum;
    size_t _bucketsNumPerWheel;

    bool _noWheels = false;

    void insertEntry(size_t delay, CallbackEntryPtr entryPtr)
    {
        //protected by bucketMutex;
        if (delay <= 0)
            return;
        delay = delay / _tickInterval + 1;
        size_t t = _ticksCounter;
        for (size_t i = 0; i < _wheelsNum; i++)
        {
            if (delay <= _bucketsNumPerWheel)
            {
                _wheels[i][delay - 1].insert(entryPtr);
                break;
            }
            if (i < (_wheelsNum - 1))
            {
                entryPtr = std::make_shared<CallbackEntry>([=]() {
                    if (delay > 0)
                    {
                        std::lock_guard<std::mutex> lock(bucketMutex_);
                        _wheels[i][(delay + (t % _bucketsNumPerWheel) - 1) % _bucketsNumPerWheel].insert(entryPtr);
                    }
                });
            }
            else
            {
                //delay is too long to put entry at valid position in wheels;
                _wheels[i][_bucketsNumPerWheel - 1].insert(entryPtr);
            }
            delay = (delay + (t % _bucketsNumPerWheel) - 1) / _bucketsNumPerWheel;
            t = t / _bucketsNumPerWheel;
        }
    }
    void eraseAfter(size_t delay, const T1 &key)
    {
        if (_noWheels)
            return;
        assert(_map.find(key) != _map.end());

        CallbackEntryPtr entryPtr;

        if (_map.find(key) != _map.end())
        {
            entryPtr = _map[key]._weakEntryPtr.lock();
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
                if (_map.find(key) != _map.end())
                {
                    auto &value = _map[key];
                    auto entryPtr = value._weakEntryPtr.lock();
                    //entryPtr is used to avoid race conditions
                    if (value.timeout > 0 &&
                        !entryPtr)
                    {
                        if (value._timeoutCallback)
                        {
                            value._timeoutCallback();
                        }
                        _map.erase(key);
                    }
                }
            };

            entryPtr = std::make_shared<CallbackEntry>(cb);
            _map[key]._weakEntryPtr = entryPtr;

            {
                std::lock_guard<std::mutex> lock(bucketMutex_);
                insertEntry(delay, entryPtr);
            }
        }
    }
};
} // namespace drogon
