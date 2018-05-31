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

#include <memory>
#include <map>
#include <mutex>
#include <thread>
#ifdef USE_STD_ANY

#include <any>
typedef std::any Any;
#define Any_cast std::any_cast

#elif USE_BOOST

#include <boost/any.hpp>
typedef boost::any Any;
#define Any_cast boost::any_cast

#else
#error,must use c++17 or boost
#endif

typedef std::map<std::string,Any> SessionMap;
namespace drogon
{
    class Session
    {
    public:
        template <typename T> T get(const std::string &key) const{
            std::lock_guard<std::mutex> lck(mutex_);
            auto it=sessionMap_.find(key);
            if(it!=sessionMap_.end())
            {
                return Any_cast<T>(it->second);
            }
            T tmp;
            return tmp;
        };
        Any &operator[](const std::string &key){
            std::lock_guard<std::mutex> lck(mutex_);
            return sessionMap_[key];
        };
        void insert(const std::string& key,const Any &obj)
        {
            std::lock_guard<std::mutex> lck(mutex_);
            sessionMap_[key]=obj;
        };
        void insert(const std::string& key,Any &&obj)
        {
            std::lock_guard<std::mutex> lck(mutex_);
            sessionMap_[key]=std::move(obj);
        }
        void erase(const std::string& key)
        {
            std::lock_guard<std::mutex> lck(mutex_);
            sessionMap_.erase(key);
        }
        bool find(const std::string& key)
        {
            std::lock_guard<std::mutex> lck(mutex_);
            if(sessionMap_.find(key) == sessionMap_.end())
            {
                return false;
            }
            return true;
        }
        void clear()
        {
            std::lock_guard<std::mutex> lck(mutex_);
            sessionMap_.clear();
        }
    protected:
        SessionMap sessionMap_;
        int timeoutInterval_;
        mutable std::mutex mutex_;
    };
    typedef std::shared_ptr<Session> SessionPtr;
}
