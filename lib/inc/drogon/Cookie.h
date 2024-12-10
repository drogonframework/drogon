/**
 *
 *  @file Cookie.h
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
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>
#include <drogon/utils/Utilities.h>
#include <cctype>
#include <string>
#include <limits>
#include <optional>
#include <string_view>

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
    Cookie(std::string key, std::string value)
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

    /**
     * @brief Set the domain of the cookie.
     */
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

    /**
     * @brief Set the path of the cookie.
     */
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

    /**
     * @brief Set the key of the cookie.
     */
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

    /**
     * @brief Set the value of the cookie.
     */
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
     *  @brief Set the partitioned status of the cookie
     */
    void setPartitioned(bool partitioned)
    {
        partitioned_ = partitioned;
        if (partitioned)
        {
            setSecure(true);
        }
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
     * @brief Check if the cookie is partitioned.
     *
     * @return true means the cookie is partitioned.
     * @return false means the cookie is not partitioned.
     */
    bool isPartitioned() const
    {
        return partitioned_;
    }

    /**
     * @brief Get the max-age of the cookie
     */
    std::optional<int> maxAge() const
    {
        return maxAge_;
    }

    /**
     * @brief Get the max-age of the cookie
     */
    std::optional<int> getMaxAge() const
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

    /**
     * @brief Compare two strings ignoring the their cases
     *
     * @param str1 string to check its value
     * @param str2 string to check against, written in lower case
     *
     * @note the function is optimized to check for cookie's samesite value
     * where we check if the value equals to a specific value we already know in
     * str2. so the function doesn't apply tolower to the second argument
     * str2 as it's always in lower case.
     *
     * @return true if both strings are equal ignoring case
     */
    static bool stricmp(const std::string_view str1,
                        const std::string_view str2)
    {
        auto str1Len{str1.length()};
        auto str2Len{str2.length()};
        if (str1Len != str2Len)
            return false;
        for (size_t idx{0}; idx < str1Len; ++idx)
        {
            auto lowerChar{tolower(str1[idx])};

            if (lowerChar != str2[idx])
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Converts a string value to its associated enum class SameSite
     * value
     */
    static SameSite convertString2SameSite(std::string_view sameSite)
    {
        if (stricmp(sameSite, "lax"))
            return Cookie::SameSite::kLax;
        if (stricmp(sameSite, "strict"))
            return Cookie::SameSite::kStrict;
        if (stricmp(sameSite, "none"))
            return Cookie::SameSite::kNone;
        if (!stricmp(sameSite, "null"))
        {
            LOG_WARN
                << "'" << sameSite
                << "' is not a valid SameSite policy. 'Null', 'Lax', 'Strict' "
                   "or "
                   "'None' are proper values. Return value is SameSite::kNull.";
        }
        return Cookie::SameSite::kNull;
    }

    /**
     * @brief Converts an enum class SameSite value to its associated string
     * value
     */
    static std::string_view convertSameSite2String(SameSite sameSite)
    {
        switch (sameSite)
        {
            case SameSite::kLax:
                return "Lax";
            case SameSite::kStrict:
                return "Strict";
            case SameSite::kNone:
                return "None";
            case SameSite::kNull:
                return "Null";
            default:
                return "UNDEFINED";
        }
    }

  private:
    trantor::Date expiresDate_{(std::numeric_limits<int64_t>::max)()};
    bool httpOnly_{true};
    bool secure_{false};
    bool partitioned_{false};
    std::string domain_;
    std::string path_;
    std::string key_;
    std::string value_;
    std::optional<int> maxAge_;
    SameSite sameSite_{SameSite::kNull};
};

}  // namespace drogon
