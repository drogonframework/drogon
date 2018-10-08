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
 *  @section DESCRIPTION
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
#define BUCKET_NUM_PER_WHEEL 100
#define TICK_INTERVAL 1.0


namespace drogon
{


class CallbackEntry
{
public:
    CallbackEntry(std::function<void()> cb):cb_(std::move(cb)){}
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

template <typename T1,typename T2>
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
    CacheMap(trantor::EventLoop *loop,
             float tickInterval=TICK_INTERVAL,
             size_t wheelsNum=WHEELS_NUM,
             size_t bucketsNumPerWheel=BUCKET_NUM_PER_WHEEL)
    :_loop(loop),
     _tickInterval(tickInterval),
     _wheelsNum(wheelsNum),
     _bucketsNumPerWheel(bucketsNumPerWheel)
    {
        _wheels.resize(_wheelsNum);
        for(int i=0;i<_wheelsNum;i++)
        {
            _wheels[i].resize(_bucketsNumPerWheel);
        }
        _timerId=_loop->runEvery(_tickInterval, [=](){
            _ticksCounter++;
            size_t t=_ticksCounter;
            size_t pow=1;
            for(int i=0;i<_wheelsNum;i++)
            {
                if((t%pow)==0)
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
                pow=pow*_bucketsNumPerWheel;
            }
        });
    };
    ~CacheMap(){
        _loop->invalidateTimer(_timerId);
        std::lock_guard<std::mutex> lock(bucketMutex_);
        for(auto & queue : _wheels)
        {
            queue.clear();
        }
    }
    typedef struct MapValue
    {
        size_t timeout=0;
        T2 value;
        std::function<void()> _timeoutCallback;
        WeakCallbackEntryPtr _weakEntryPtr;
    }MapValue;
    void insert(const T1& key,T2&& value,size_t timeout=0,std::function<void()> timeoutCallback=std::function<void()>())
    {
        if(timeout>0)
        {

            std::lock_guard<std::mutex> lock(mtx_);
            _map[key].value=std::forward<T2>(value);
            _map[key].timeout=timeout;
            _map[key]._timeoutCallback=std::move(timeoutCallback);

            eraseAfter(timeout,key);
        }
        else
        {
            std::lock_guard<std::mutex> lock(mtx_);
            _map[key].value=std::forward<T2>(value);
            _map[key].timeout=timeout;
            _map[key]._timeoutCallback=std::function<void()>();
            _map[key]._weakEntryPtr=WeakCallbackEntryPtr();
        }
    }
    T2& operator [](const T1& key){
        int timeout=0;

        std::lock_guard<std::mutex> lock(mtx_);
        if(_map.find(key)!=_map.end())
        {
            timeout=_map[key].timeout;
        }


        if(timeout>0)
            eraseAfter(timeout,key);


        return _map[key].value;
    }
    bool find(const T1& key)
    {
        int timeout=0;
        bool flag=false;

        std::lock_guard<std::mutex> lock(mtx_);
        if(_map.find(key)!=_map.end())
        {
            timeout=_map[key].timeout;
            flag=true;
        }


        if(timeout>0)
            eraseAfter(timeout,key);


        return flag;
    }
    void erase(const T1& key)
    {
        //in this case,we don't evoke the timeout callback;

        std::lock_guard<std::mutex> lock(mtx_);
        _map.erase(key);

    }

private:
    std::unordered_map< T1,MapValue > _map;

    std::vector<CallbackBucketQueue> _wheels;

    std::atomic<size_t> _ticksCounter=ATOMIC_VAR_INIT(0);

    std::mutex mtx_;
    std::mutex bucketMutex_;
    trantor::TimerId _timerId;
    trantor::EventLoop* _loop;

    float _tickInterval;
    size_t _wheelsNum;
    size_t _bucketsNumPerWheel;


    void inertEntry(int delay,CallbackEntryPtr entryPtr)
    {
        //protected by bucketMutex;
        if(delay<=0)
            return;
        size_t t=_ticksCounter;
        for(int i=0;i<_wheelsNum;i++)
        {
            if(delay<=_bucketsNumPerWheel)
            {
                _wheels[i][delay-1].insert(entryPtr);
                break;
            }
            if(i<(_wheelsNum-1))
            {
                entryPtr=std::make_shared<CallbackEntry>([=](){
                    if(delay>0)
                    {
                        std::lock_guard<std::mutex> lock(bucketMutex_);
                        _wheels[i][(delay+(t%_bucketsNumPerWheel)-1)%_bucketsNumPerWheel].insert(entryPtr);
                    }
                });
            }
            else
            {
                //delay is too long to put entry at valid position in wheels;
                _wheels[i][_bucketsNumPerWheel-1].insert(entryPtr);
            }
            delay=(delay+(t%_bucketsNumPerWheel)-1)/_bucketsNumPerWheel;
            t=t/_bucketsNumPerWheel;
        }
    }
    void eraseAfter(int delay,const T1& key)
    {
        assert(_map.find(key)!=_map.end());


        CallbackEntryPtr entryPtr;

        if(_map.find(key)!=_map.end())
        {
            entryPtr=_map[key]._weakEntryPtr.lock();
        }

        if(entryPtr)
        {
            std::lock_guard<std::mutex> lock(bucketMutex_);
            inertEntry(delay,entryPtr);
        }
        else
        {
            std::function<void ()>cb=[=](){
                std::lock_guard<std::mutex> lock(mtx_);
                if(_map.find(key)!=_map.end())
                {
                    auto & value=_map[key];
                    if(value.timeout>0)
                    {
                        if(value._timeoutCallback)
                        {
                            value._timeoutCallback();
                        }
                        _map.erase(key);
                    }
                }

            };

            entryPtr=std::make_shared<CallbackEntry>(cb);
            _map[key]._weakEntryPtr=entryPtr;

            {
                std::lock_guard<std::mutex> lock(bucketMutex_);
                inertEntry(delay,entryPtr);
            }
        }
    }
};
}
