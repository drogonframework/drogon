/**
 *
 *  Cookis.h
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

#include <string>
#include <trantor/utils/Date.h>

namespace drogon
{
/**
 * @brief this class represents a cookie entity.
 */
class Cookie
{
  public:
    /// Constructor
    /**
     * @param key key of the cookie
     * @param value value of the cookie
     */
    Cookie(const std::string &key, const std::string &value)
        : _key(key), _value(value)
    {
    }
    Cookie() = default;

    /**
     * @brief Set the Expires Date
     *
     * @param date The expiration date
     */
    void setExpiresDate(const trantor::Date &date)
    {
        _expiresDate = date;
    }

    /**
     * @brief Set if the cookie is HTTP only.
     */
    void setHttpOnly(bool only)
    {
        _httpOnly = only;
    }

    /**
     * @brief Set if the cookie is secure.
     */
    void setSecure(bool secure)
    {
        _secure = secure;
    }

    /**
     * @brief Set the domain of the cookie.
     */
    void setDomain(const std::string &domain)
    {
        _domain = domain;
    }
    void setDomain(std::string &&domain)
    {
        _domain = std::move(domain);
    }

    /**
     * @brief Set the path of the cookie.
     */
    void setPath(const std::string &path)
    {
        _path = path;
    }
    void setPath(std::string &&path)
    {
        _path = std::move(path);
    }

    /**
     * @brief Set the key of the cookie.
     */
    void setKey(const std::string &key)
    {
        _key = key;
    }
    void setKey(std::string &&key)
    {
        _key = std::move(key);
    }
    /**
     * @brief Set the value of the cookie.
     */
    void setValue(const std::string &value)
    {
        _value = value;
    }
    void setValue(std::string &&value)
    {
        _value = std::move(value);
    }

    /**
     * @brief Get the string value of the cookie
     */
    std::string cookieString() const;

    /**
     * @brief Get the string value of the cookie
     */
    std::string getCookieString() const
    {
        return cookieString();
    }

    /**
     * @brief Get the expiration date of the cookie
     */
    const trantor::Date &expiresDate() const
    {
        return _expiresDate;
    }

    /**
     * @brief Get the expiration date of the cookie
     */
    const trantor::Date &getExpiresDate() const
    {
        return _expiresDate;
    }

    /**
     * @brief Get the domain of the cookie
     */
    const std::string &domain() const
    {
        return _domain;
    }

    /**
     * @brief Get the domain of the cookie
     */
    const std::string &getDomain() const
    {
        return _domain;
    }

    /**
     * @brief Get the path of the cookie
     */
    const std::string &path() const
    {
        return _path;
    }

    /**
     * @brief Get the path of the cookie
     */
    const std::string &getPath() const
    {
        return _path;
    }

    /**
     * @brief Get the keyword of the cookie
     */
    const std::string &key() const
    {
        return _key;
    }

    /**
     * @brief Get the keyword of the cookie
     */
    const std::string &getKey() const
    {
        return _key;
    }

    /**
     * @brief Get the value of the cookie
     */
    const std::string &value() const
    {
        return _value;
    }

    /**
     * @brief Get the value of the cookie
     */
    const std::string &getValue() const
    {
        return _value;
    }

    /**
     * @brief Check if the cookie is empty
     *
     * @return true means the cookie is not empty
     * @return false means the cookie is empty
     */
    operator bool() const
    {
        return (!_key.empty()) && (!_value.empty());
    }

    /**
     * @brief Check if the cookie is HTTP only
     *
     * @return true means the cookie is HTTP only
     * @return false means the cookie is not HTTP only
     */
    bool isHttpOnly() const
    {
        return _httpOnly;
    }

    /**
     * @brief Check if the cookie is secure.
     *
     * @return true means the cookie is secure.
     * @return false means the cookie is not secure.
     */
    bool isSecure() const
    {
        return _secure;
    }

  private:
    trantor::Date _expiresDate;
    bool _httpOnly = true;
    bool _secure = false;
    std::string _domain;
    std::string _path;
    std::string _key;
    std::string _value;
};

}  // namespace drogon
