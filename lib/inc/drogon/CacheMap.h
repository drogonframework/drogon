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
#include <map>
#include <mutex>
#include <deque>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
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
    /// @param interval
    /// timer step（seconds）
    /// @param limit
    /// tht max timeout value of the cache (seconds)
    CacheMap(trantor::EventLoop *loop,int interval,int limit)
    :timeInterval_(interval),
     _limit(limit),
     _loop(loop)
    {
        bucketCount_=limit/interval+1;
        event_bucket_queue_.resize(bucketCount_);
        _timerId=_loop->runEvery(interval, [=](){
            CallbackBucket tmp;
            {
                std::lock_guard<std::mutex> lock(bucketMutex_);
                //use tmp val to make this critical area as short as possible.
                event_bucket_queue_.front().swap(tmp);
                event_bucket_queue_.pop_front();
                event_bucket_queue_.push_back(CallbackBucket());
            }
        });
    };
    ~CacheMap(){
        _loop->invalidateTimer(_timerId);
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
    CallbackBucketQueue event_bucket_queue_;

    std::mutex mtx_;
    std::mutex bucketMutex_;
    int bucketCount_;
    int timeInterval_;
    int _limit;
    trantor::TimerId _timerId;
    trantor::EventLoop* _loop;

    void eraseAfter(int delay,const T1& key)
    {
        assert(_map.find(key)!=_map.end());

        size_t bucketIndexToPush;
        size_t bucketNum = size_t(delay / timeInterval_) + 1;
        size_t queue_size = event_bucket_queue_.size();


        if (bucketNum >= queue_size)
        {
            bucketIndexToPush = queue_size - 1;
        }
        else
        {
            bucketIndexToPush = bucketNum;
        }

        CallbackEntryPtr entryPtr;

        if(_map.find(key)!=_map.end())
        {
            entryPtr=_map[key]._weakEntryPtr.lock();
        }

        if(entryPtr)
        {
            std::lock_guard<std::mutex> lock(bucketMutex_);
            event_bucket_queue_[bucketIndexToPush].insert(entryPtr);
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
                event_bucket_queue_[bucketIndexToPush].insert(entryPtr);
            }
        }
    }
};
}
