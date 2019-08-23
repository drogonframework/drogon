/**
 *
 *  Session.h
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

#include <drogon/utils/any.h>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace drogon
{
class Session
{
  public:
    template <typename T>
    const T &get(const std::string &key) const
    {
        const static T nullVal = T();
        std::lock_guard<std::mutex> lck(_mutex);
        auto it = _sessionMap.find(key);
        if (it != _sessionMap.end())
        {
            return *(any_cast<T>(&(it->second)));
        }
        return nullVal;
    };
    any &operator[](const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        return _sessionMap[key];
    };
    void insert(const std::string &key, const any &obj)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap[key] = obj;
    };
    void insert(const std::string &key, any &&obj)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap[key] = std::move(obj);
    }
    void erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap.erase(key);
    }
    bool find(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        if (_sessionMap.find(key) == _sessionMap.end())
        {
            return false;
        }
        return true;
    }
    void clear()
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap.clear();
    }
    const std::string &sessionId() const
    {
        return _sessionId;
    }
    bool needSetToClient() const
    {
        return _needToSet;
    }
    void hasSet()
    {
        _needToSet = false;
    }
    Session(const std::string &id, bool needToSet)
        : _sessionId(id), _needToSet(needToSet)
    {
    }
    Session() = delete;

  private:
    typedef std::map<std::string, any> SessionMap;
    SessionMap _sessionMap;
    mutable std::mutex _mutex;
    std::string _sessionId;
    bool _needToSet = false;
};

typedef std::shared_ptr<Session> SessionPtr;

}  // namespace drogon
