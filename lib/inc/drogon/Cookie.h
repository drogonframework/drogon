/**
 *
 *  @file Cookis.h
 *  @author An Tao
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

#include <drogon/exports.h>
#include <drogon/utils/optional.h>
#include <trantor/utils/Date.h>
#include <string>
#include <limits>

namespace drogon
{
/**
 * @brief this class represents a cookie entity.
 */
class DROGON_EXPORT Cookie
{
  public:
    /// Constructor
    /**
     * @param key key of the cookie
     * @param value value of the cookie
     */
    Cookie(const std::string &key, const std::string &value)
        : key_(key), value_(value)
    {
    }
    Cookie(std::string &&key, std::string &&value)
        : key_(std::move(key)), value_(std::move(value))
    {
    }
    Cookie() = default;
    enum class SameSite
    {
        kNull,
        kLax,
        kStrict,
        kNone
    };
    /**
     * @brief Set the Expires Date
     *
     * @param date The expiration date
     */
    void setExpiresDate(const trantor::Date &date)
    {
        expiresDate_ = date;
    }

    /**
     * @brief Set if the cookie is HTTP only.
     */
    void setHttpOnly(bool only)
    {
        httpOnly_ = only;
    }

    /**
     * @brief Set if the cookie is secure.
     */
    void setSecure(bool secure)
    {
        secure_ = secure;
    }

    /**
     * @brief Set the domain of the cookie.
     */
    void setDomain(const std::string &domain)
    {
        domain_ = domain;
    }
    void setDomain(std::string &&domain)
    {
        domain_ = std::move(domain);
    }

    /**
     * @brief Set the path of the cookie.
     */
    void setPath(const std::string &path)
    {
        path_ = path;
    }
    void setPath(std::string &&path)
    {
        path_ = std::move(path);
    }

    /**
     * @brief Set the key of the cookie.
     */
    void setKey(const std::string &key)
    {
        key_ = key;
    }
    void setKey(std::string &&key)
    {
        key_ = std::move(key);
    }
    /**
     * @brief Set the value of the cookie.
     */
    void setValue(const std::string &value)
    {
        value_ = value;
    }
    void setValue(std::string &&value)
    {
        value_ = std::move(value);
    }
    /**
     * @brief Set the max-age of the cookie.
     */
    void setMaxAge(int value)
    {
        maxAge_ = value;
    }
    /**
     * @brief Set the same site of the cookie.
     */
    void setSameSite(SameSite sameSite)
    {
        sameSite_ = sameSite;
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
        return expiresDate_;
    }

    /**
     * @brief Get the expiration date of the cookie
     */
    const trantor::Date &getExpiresDate() const
    {
        return expiresDate_;
    }

    /**
     * @brief Get the domain of the cookie
     */
    const std::string &domain() const
    {
        return domain_;
    }

    /**
     * @brief Get the domain of the cookie
     */
    const std::string &getDomain() const
    {
        return domain_;
    }

    /**
     * @brief Get the path of the cookie
     */
    const std::string &path() const
    {
        return path_;
    }

    /**
     * @brief Get the path of the cookie
     */
    const std::string &getPath() const
    {
        return path_;
    }

    /**
     * @brief Get the keyword of the cookie
     */
    const std::string &key() const
    {
        return key_;
    }

    /**
     * @brief Get the keyword of the cookie
     */
    const std::string &getKey() const
    {
        return key_;
    }

    /**
     * @brief Get the value of the cookie
     */
    const std::string &value() const
    {
        return value_;
    }

    /**
     * @brief Get the value of the cookie
     */
    const std::string &getValue() const
    {
        return value_;
    }

    /**
     * @brief Check if the cookie is empty
     *
     * @return true means the cookie is not empty
     * @return false means the cookie is empty
     */
    operator bool() const
    {
        return (!key_.empty()) && (!value_.empty());
    }

    /**
     * @brief Check if the cookie is HTTP only
     *
     * @return true means the cookie is HTTP only
     * @return false means the cookie is not HTTP only
     */
    bool isHttpOnly() const
    {
        return httpOnly_;
    }

    /**
     * @brief Check if the cookie is secure.
     *
     * @return true means the cookie is secure.
     * @return false means the cookie is not secure.
     */
    bool isSecure() const
    {
        return secure_;
    }

    /**
     * @brief Get the max-age of the cookie
     */
    optional<int> maxAge() const
    {
        return maxAge_;
    }

    /**
     * @brief Get the max-age of the cookie
     */
    optional<int> getMaxAge() const
    {
        return maxAge_;
    }

    /**
     * @brief Get the same site of the cookie
     */
    SameSite sameSite() const
    {
        return sameSite_;
    }

    /**
     * @brief Get the same site of the cookie
     */
    SameSite getSameSite() const
    {
        return sameSite_;
    }

  private:
    trantor::Date expiresDate_{(std::numeric_limits<int64_t>::max)()};
    bool httpOnly_{true};
    bool secure_{false};
    std::string domain_;
    std::string path_;
    std::string key_;
    std::string value_;
    optional<int> maxAge_;
    SameSite sameSite_{SameSite::kNull};
};

}  // namespace drogon
