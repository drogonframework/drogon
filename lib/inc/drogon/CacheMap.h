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

#include <map>
#include <trantor/net/EventLoop.h>
#include <mutex>
#include <deque>
#include <vector>
#include <set>

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

typedef std::set<CallbackEntryPtr> CallbackBucket;
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
        _loop->runEvery(interval, [=](){
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
    typedef struct MapValue
    {
        int timeout=0;
        T2 value;
        std::function<void()> _timeoutCallback;
    }MapValue;
    void insert(const T1& key,T2&& value,int timeout=0,std::function<void()> timeoutCallback=std::function<void()>())
    {
        if(timeout>0)
        {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                map_[key].value=std::forward<T2>(value);
                map_[key].timeout=timeout;
                map_[key]._timeoutCallback=std::move(timeoutCallback);
            }
            eraseAfter(timeout,key);
        }
        else
        {
            std::lock_guard<std::mutex> lock(mtx_);
            map_[key].value=std::forward<T2>(value);
            map_[key].timeout=timeout;
        }
    }
    T2& operator [](const T1& key){
        int timeout=0;
        std::lock_guard<std::mutex> lock(mtx_);
        if(map_.find(key)!=map_.end())
        {
            timeout=map_[key].timeout;
        }
        if(timeout>0)
            eraseAfter(timeout,key);


        return map_[key].value;
    }
    bool find(const T1& key)
    {
        int timeout=0;
        bool flag=false;
        std::lock_guard<std::mutex> lock(mtx_);
        if(map_.find(key)!=map_.end())
        {
            timeout=map_[key].timeout;
            flag=true;
        }
        if(timeout>0)
            eraseAfter(timeout,key);


        return flag;
    }
    void erase(const T1& key)
    {
        //in this case,we don't evoke the timeout callback;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            map_.erase(key);
        }
        {
            std::lock_guard<std::mutex> lock(weakPtrMutex_);
            weak_entry_map_.erase(key);
        }
    }
protected:
    std::map< T1,MapValue > map_;
    CallbackBucketQueue event_bucket_queue_;
    std::map< T1, WeakCallbackEntryPtr > weak_entry_map_;
    std::mutex mtx_;
    std::mutex weakPtrMutex_;
    std::mutex bucketMutex_;
    int bucketCount_;
    int timeInterval_;
    int _limit;
    trantor::EventLoop* _loop;

    void eraseAfter(int delay,const T1& key)
    {
        uint32_t bucketIndexToPush;
        uint32_t bucketNum = uint32_t(delay / timeInterval_) + 1;
        uint32_t queue_size = event_bucket_queue_.size();

        if (bucketNum >= queue_size)
        {
            bucketIndexToPush = queue_size - 1;
        }
        else
        {
            bucketIndexToPush = bucketNum;
        }

        CallbackEntryPtr entryPtr;
        {
            std::lock_guard<std::mutex> lock(weakPtrMutex_);
            if(weak_entry_map_.find(key)!=weak_entry_map_.end())
            {
                entryPtr=weak_entry_map_[key].lock();
            }
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
                std::lock_guard<std::mutex> lock1(weakPtrMutex_);
                WeakCallbackEntryPtr tmpWeakPtr;
                if(weak_entry_map_.find(key)!=weak_entry_map_.end())
                {
                    tmpWeakPtr=weak_entry_map_[key];

                    if(!tmpWeakPtr.lock())
                    {
                        weak_entry_map_.erase(key);
                        if(map_.find(key)!=map_.end())
                        {
                            if(map_[key]._timeoutCallback)
                            {
                                map_[key]._timeoutCallback();
                            }
                            map_.erase(key);
                        }
                    }

                }

            };
            entryPtr=std::make_shared<CallbackEntry>(cb);
            {
                std::lock_guard<std::mutex> lock(weakPtrMutex_);
                weak_entry_map_[key] = WeakCallbackEntryPtr(entryPtr);
            }
            {
                std::lock_guard<std::mutex> lock(bucketMutex_);
                event_bucket_queue_[bucketIndexToPush].insert(entryPtr);
            }
        }
    }
};
}