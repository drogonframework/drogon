/**
 *
 *  HttpViewData.h
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

#include <trantor/utils/Logger.h>
#include <drogon/config.h>
#include <trantor/utils/MsgBuffer.h>

#include <unordered_map>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdarg.h>

namespace drogon
{

/// This class represents the data set displayed in views.
class HttpViewData
{
  public:
    /// The function template is used to get an item in the data set by the @param key.
    template <typename T>
    const T &get(const std::string &key, T &&nullVal = T()) const
    {
        auto it = _viewData.find(key);
        if (it != _viewData.end())
        {
            try
            {
                return *(any_cast<T>(&(it->second)));
            }
            catch (std::exception &e)
            {
                LOG_ERROR << e.what();
            }
        }
        return nullVal;
    }

    /// Insert an item identified by the @param key into the data set;
    void insert(const std::string &key, any &&obj)
    {
        _viewData[key] = std::move(obj);
    }
    void insert(const std::string &key, const any &obj)
    {
        _viewData[key] = obj;
    }

    /// Insert an item identified by the @param key into the data set; The item will be converted to a string.
    template <typename T>
    void insertAsString(const std::string &key, T &&val)
    {
        std::stringstream ss;
        ss << val;
        _viewData[key] = ss.str();
    }

    /// Insert a formated string identified by the @param key.
    void insertFormattedString(const std::string &key,
                               const char *format, ...)
    {
        std::string strBuffer;
        strBuffer.resize(1024);
        va_list ap, backup_ap;
        va_start(ap, format);
        va_copy(backup_ap, ap);
        auto result = vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
        va_end(backup_ap);
        if ((result >= 0) && ((std::string::size_type)result < strBuffer.size()))
        {
            strBuffer.resize(result);
        }
        else
        {
            while (true)
            {
                if (result < 0)
                {
                    // Older snprintf() behavior. Just try doubling the buffer size
                    strBuffer.resize(strBuffer.size() * 2);
                }
                else
                {
                    strBuffer.resize(result + 1);
                }

                va_copy(backup_ap, ap);
                auto result = vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
                va_end(backup_ap);

                if ((result >= 0) && ((std::string::size_type)result < strBuffer.size()))
                {
                    strBuffer.resize(result);
                    break;
                }
            }
        }
        va_end(ap);
        _viewData[key] = std::move(strBuffer);
    }

    /// Get the 'any' object by the @param key.
    any &operator[](const std::string &key) const
    {
        return _viewData[key];
    }

  protected:
    typedef std::unordered_map<std::string, any> ViewDataMap;
    mutable ViewDataMap _viewData;
};

} // namespace drogon
