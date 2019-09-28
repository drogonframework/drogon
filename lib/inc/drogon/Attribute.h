/**
 *
 *  Attribute.h
 *  armstrong@sweelia.com
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

namespace drogon
{
/**
 * @brief This class represents a attribute stored in the request context.
 * One can get or set any type of data to a attribute object.
 */
class Attributes
{
  public:
    /**
     * @brief Get the data identified by the key parameter.
     * @note if the data is not found, a default value is returned.
     * For example:
     * @code
       auto &userName = attributesPtr->get<std::string>("user name");
       @endcode
     */
    template <typename T>
    const T &get(const std::string &key) const
    {
        const static T nullVal = T();
        auto it = _attributesMap.find(key);
        if (it != _attributesMap.end())
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
        return _attributesMap[key];
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       attributesPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, const any &obj)
    {
        _attributesMap[key] = obj;
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       attributesPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, any &&obj)
    {
        _attributesMap[key] = std::move(obj);
    }

    /**
     * @brief Erase the data identified by the given key.
     */
    void erase(const std::string &key)
    {
        _attributesMap.erase(key);
    }

    /**
     * @brief Retrun true if the data identified by the key exists.
     */
    bool find(const std::string &key)
    {
        if (_attributesMap.find(key) == _attributesMap.end())
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Clear all attributes.
     */
    void clear()
    {
        _attributesMap.clear();
    }

    /**
     * @brief Constructor, usually called by the framework
     */
    Attributes()
    {
    }

  private:
    typedef std::map<std::string, any> AttributesMap;
    AttributesMap _attributesMap;
};

typedef std::shared_ptr<Attributes> AttributesPtr;

}  // namespace drogon
