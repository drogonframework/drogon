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
class Cookie
{
  public:
    Cookie(const std::string &key, const std::string &value) : _key(key), _value(value)
    {
    }
    Cookie() = default;
    ~Cookie()
    {
    }
    void setExpiresDate(const trantor::Date &date)
    {
        _expiresDate = date;
    }
    void setHttpOnly(bool only)
    {
        _httpOnly = only;
    }
    void setSecure(bool secure)
    {
        _secure = secure;
    }
    void setDomain(const std::string &domain)
    {
        _domain = domain;
    }
    void setPath(const std::string &path)
    {
        _path = path;
    }
    void setKey(const std::string &key)
    {
        _key = key;
    }
    void setValue(const std::string &value)
    {
        _value = value;
    }

    std::string cookieString() const;
    std::string getCookieString() const
    {
        return cookieString();
    }

    const trantor::Date &expiresDate() const
    {
        return _expiresDate;
    }
    const trantor::Date &getExpiresDate() const
    {
        return _expiresDate;
    }

    const std::string &domain() const
    {
        return _domain;
    }
    const std::string &getDomain() const
    {
        return _domain;
    }

    const std::string &path() const
    {
        return _path;
    }
    const std::string &getPath() const
    {
        return _path;
    }

    const std::string &key() const
    {
        return _key;
    }
    const std::string &getKey() const
    {
        return _key;
    }

    const std::string &value() const
    {
        return _value;
    }
    const std::string &getValue() const
    {
        return _value;
    }

    operator bool() const
    {
        return (!_key.empty()) && (!_value.empty());
    }
    bool isHttpOnly() const
    {
        return _httpOnly;
    }
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
