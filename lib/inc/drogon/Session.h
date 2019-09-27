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
#include <trantor/utils/Logger.h>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace drogon
{
/**
 * @brief This class represents a session stored in the framework.
 * One can get or set any type of data to a session object.
 */
class Session
{
  public:
    /**
     * @brief Get the data identified by the key parameter.
     * @note if the data is not found, a default value is returned.
     * For example:
     * @code
       auto &userName = sessionPtr->get<std::string>("user name");
       @endcode
     */
    template <typename T>
    const T &get(const std::string &key) const
    {
        const static T nullVal = T();
        std::lock_guard<std::mutex> lck(_mutex);
        auto it = _sessionMap.find(key);
        if (it != _sessionMap.end())
        {
            if (typeid(T) == it->second.type())
            {
                return *(any_cast<T>(&(it->second)));
            }
            else
            {
                LOG_ERROR << "Bad type";
            }
        }
        return nullVal;
    }

    /**
     * @brief Get the 'any' object identified by the given key
     */
    any &operator[](const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        return _sessionMap[key];
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       sessionPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, const any &obj)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap[key] = obj;
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       sessionPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, any &&obj)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap[key] = std::move(obj);
    }

    /**
     * @brief Erase the data identified by the given key.
     */
    void erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap.erase(key);
    }

    /**
     * @brief Retrun true if the data identified by the key exists.
     */
    bool find(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(_mutex);
        if (_sessionMap.find(key) == _sessionMap.end())
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Clear all data in the session.
     */
    void clear()
    {
        std::lock_guard<std::mutex> lck(_mutex);
        _sessionMap.clear();
    }

    /**
     * @brief Get the session ID of the current session.
     */
    const std::string &sessionId() const
    {
        return _sessionId;
    }

    /**
     * @brief If the session ID needs to be set to the client through cookie,
     * return true
     */
    bool needSetToClient() const
    {
        return _needToSet;
    }
    /**
     * @brief Change the state of the session, usually called by the framework
     */
    void hasSet()
    {
        _needToSet = false;
    }
    /**
     * @brief Constructor, usually called by the framework
     */
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
