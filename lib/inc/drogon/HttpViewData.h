/**
 *
 *  @file HttpViewData.h
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
#include <drogon/utils/string_view.h>
#include <drogon/utils/any.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <stdarg.h>
#include <stdio.h>
#include <type_traits>

namespace drogon
{
/// This class represents the data set displayed in views.
class DROGON_EXPORT HttpViewData
{
  public:
    /// The function template is used to get an item in the data set by the key
    /// parameter.
    template <typename T>
    const T &get(const std::string &key) const
    {
        const static T nullVal = T();
        auto it = viewData_.find(key);
        if (it != viewData_.end())
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

    /// Insert an item identified by the key parameter into the data set;
    void insert(const std::string &key, any &&obj)
    {
        viewData_[key] = std::move(obj);
    }
    void insert(const std::string &key, const any &obj)
    {
        viewData_[key] = obj;
    }

    /// Insert an item identified by the key parameter into the data set; The
    /// item is converted to a string.
    template <typename T>
    void insertAsString(const std::string &key, T &&val)
    {
        std::stringstream ss;
        ss << val;
        viewData_[key] = ss.str();
    }

    /// Insert a formated string identified by the key parameter.
    void insertFormattedString(const std::string &key, const char *format, ...)
    {
        std::string strBuffer;
        strBuffer.resize(128);
        va_list ap, backup_ap;
        va_start(ap, format);
        va_copy(backup_ap, ap);
        auto result = vsnprintf((char *)strBuffer.data(),
                                strBuffer.size(),
                                format,
                                backup_ap);
        va_end(backup_ap);
        if ((result >= 0) &&
            ((std::string::size_type)result < strBuffer.size()))
        {
            strBuffer.resize(result);
        }
        else
        {
            while (true)
            {
                if (result < 0)
                {
                    // Older snprintf() behavior. Just try doubling the buffer
                    // size
                    strBuffer.resize(strBuffer.size() * 2);
                }
                else
                {
                    strBuffer.resize(result + 1);
                }

                va_copy(backup_ap, ap);
                result = vsnprintf((char *)strBuffer.data(),
                                   strBuffer.size(),
                                   format,
                                   backup_ap);
                va_end(backup_ap);

                if ((result >= 0) &&
                    ((std::string::size_type)result < strBuffer.size()))
                {
                    strBuffer.resize(result);
                    break;
                }
            }
        }
        va_end(ap);
        viewData_[key] = std::move(strBuffer);
    }

    /// Get the 'any' object by the key parameter.
    any &operator[](const std::string &key) const
    {
        return viewData_[key];
    }

    /// Translate some special characters to HTML format
    /**
     * such as:
     * @code
       " --> &quot;
       & --> &amp;
       < --> &lt;
       > --> &gt;
       @endcode
     */
    static std::string htmlTranslate(const char *str, size_t length);
    static std::string htmlTranslate(const string_view &str)
    {
        return htmlTranslate(str.data(), str.length());
    }

    static bool needTranslation(const string_view &str)
    {
        for (auto const &c : str)
        {
            switch (c)
            {
                case '"':
                case '&':
                case '<':
                case '>':
                    return true;
                default:
                    continue;
            }
        }
        return false;
    }

  protected:
    using ViewDataMap = std::unordered_map<std::string, any>;
    mutable ViewDataMap viewData_;
};

}  // namespace drogon
