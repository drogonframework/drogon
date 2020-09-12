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
        std::lock_guard<std::mutex> lck(mutex_);
        auto it = sessionMap_.find(key);
        if (it != sessionMap_.end())
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
     * @brief Modify the data identified by the key parameter.
     *
     * @tparam T the type of the data.
     * @param key
     * @param handler A callable that can modify the data.
     *
     * @note This function is multiple-thread safe. if the data identified by
     * the key doesn't exist, a new one is created and passed to the handler.
     * The changing of the data is protected by the mutex which protects the
     * whole session, if one wants to access the data by a separated mutex,
     * please create a mutex for it and do something like:
     *
     * @code
       // protected by the mutex of the session
       auto &anydata = (*sessionPtr)[key];
       // protected by the mutex of the data
       std::lock_guard<std::mutex> lck(mutexForTheData);
       if(anydata.has_value())
       {
           doSomething(*any_cast<DataType>(&anydata));
       }
       else
       {
           auto data = createData();
           doSomething(data);
           anydata = std::move(data);
       }
       @endcode
     */
    template <typename T>
    void modify(const std::string &key, const std::function<void(T &)> &handler)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        auto it = sessionMap_.find(key);
        if (it != sessionMap_.end())
        {
            if (typeid(T) == it->second.type())
            {
                handler(*(any_cast<T>(&(it->second))));
            }
            else
            {
                LOG_ERROR << "Bad type";
            }
        }
        else
        {
            auto iterm = T();
            handler(iterm);
            sessionMap_[key] = std::move(iterm);
        }
    }
    /**
     * @brief Modify the session data.
     *
     * @tparam Callable: The signature of the callable should be equivalent to
     * `void (SessionMap &)` or `void (const SessionMap &)`
     * @param handler A callable that can modify the sessionMap_ inside the
     * session.
     * @note This function is multiple-thread safe.
     */
    template <typename Callable>
    void modify(Callable &&handler)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        handler(sessionMap_);
    }
    /**
     * @brief Get the 'any' object identified by the given key
     */
    any &operator[](const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        return sessionMap_[key];
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
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_[key] = obj;
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
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_[key] = std::move(obj);
    }

    /**
     * @brief Erase the data identified by the given key.
     */
    void erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_.erase(key);
    }

    /**
     * @brief Retrun true if the data identified by the key exists.
     */
    bool find(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        if (sessionMap_.find(key) == sessionMap_.end())
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
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_.clear();
    }

    /**
     * @brief Get the session ID of the current session.
     */
    std::string sessionId() const
    {
        std::lock_guard<std::mutex> lck(mutex_);
        return sessionId_;
    }

    /**
     * @brief Let the framework create a new session ID for this session and set
     * it to the client.
     * @note This method does not change the session ID now.
     */
    void changeSessionIdToClient()
    {
        needToChange_ = true;
        needToSet_ = true;
    }
    Session() = delete;

  private:
    using SessionMap = std::map<std::string, any>;
    SessionMap sessionMap_;
    mutable std::mutex mutex_;
    std::string sessionId_;
    bool needToSet_{false};
    bool needToChange_{false};
    friend class SessionManager;
    friend class HttpAppFrameworkImpl;
    /**
     * @brief Constructor, usually called by the framework
     */
    Session(const std::string &id, bool needToSet)
        : sessionId_(id), needToSet_(needToSet)
    {
    }
    /**
     * @brief Change the state of the session, usually called by the framework
     */
    void hasSet()
    {
        needToSet_ = false;
    }
    /**
     * @brief If the session ID needs to be changed.
     *
     */
    bool needToChangeSessionId() const
    {
        return needToChange_;
    }
    /**
     * @brief If the session ID needs to be set to the client through cookie,
     * return true
     */
    bool needSetToClient() const
    {
        return needToSet_;
    }
    void setSessionId(const std::string &id)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionId_ = id;
        needToChange_ = false;
    }
};

using SessionPtr = std::shared_ptr<Session>;

}  // namespace drogon
