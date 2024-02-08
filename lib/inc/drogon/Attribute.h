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

#include <trantor/utils/Logger.h>
#include <map>
#include <memory>
#include <any>

namespace drogon
{
/**
 * @brief This class represents the attributes stored in the HTTP request.
 * One can add/get any type of data to/from an Attributes object.
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
        static const T nullVal = T();
        auto it = attributesMap_.find(key);
        if (it != attributesMap_.end())
        {
            if (typeid(T) == it->second.type())
            {
                return *(std::any_cast<T>(&(it->second)));
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
    std::any &operator[](const std::string &key)
    {
        return attributesMap_[key];
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       attributesPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, const std::any &obj)
    {
        attributesMap_[key] = obj;
    }

    /**
     * @brief Insert a key-value pair
     * @note here the any object can be created implicitly. for example
     * @code
       attributesPtr->insert("user name", userNameString);
       @endcode
     */
    void insert(const std::string &key, std::any &&obj)
    {
        attributesMap_[key] = std::move(obj);
    }

    /**
     * @brief Erase the data identified by the given key.
     */
    void erase(const std::string &key)
    {
        attributesMap_.erase(key);
    }

    /**
     * @brief Return true if the data identified by the key exists.
     */
    bool find(const std::string &key)
    {
        if (attributesMap_.find(key) == attributesMap_.end())
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
        attributesMap_.clear();
    }

    /**
     * @brief Constructor, usually called by the framework
     */
    Attributes() = default;

  private:
    using AttributesMap = std::map<std::string, std::any>;
    AttributesMap attributesMap_;
};

using AttributesPtr = std::shared_ptr<Attributes>;

}  // namespace drogon
