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

#include <trantor/utils/Date.h>
#include <string>

namespace drogon
{

class Cookie
{
  public:
    Cookie(const std::string &key, const std::string &value)
        : _key(key),
          _value(value)
    {
    }
    Cookie() = default;
    ~Cookie() {}
    void setExpiresDate(const trantor::Date &date) { _expiresDate = date; }
    void setHttpOnly(bool only) { _httpOnly = only; }
    void setEnsure(bool ensure) { _ensure = ensure; }
    void setDomain(const std::string &domain) { _domain = domain; }
    void setPath(const std::string &path) { _path = path; }
    void setKey(const std::string &key) { _key = key; }
    void setValue(const std::string &value) { _value = value; }

    const std::string cookieString() const;
    const trantor::Date &expiresDate() const { return _expiresDate; }
    bool httpOnly() const { return _httpOnly; }
    bool ensure() const { return _ensure; }
    const std::string &domain() const { return _domain; }
    const std::string &path() const { return _path; }
    const std::string &key() const { return _key; }
    const std::string &value() const { return _value; }

  private:
    trantor::Date _expiresDate;
    bool _httpOnly = true;
    bool _ensure = false;
    std::string _domain;
    std::string _path;
    std::string _key;
    std::string _value;
};

} // namespace drogon
