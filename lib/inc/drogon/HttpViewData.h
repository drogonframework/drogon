/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
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

typedef std::unordered_map<std::string,any> ViewDataMap;
namespace drogon
{
    class HttpViewData
    {
    public:
        template <typename T>
        const T get(const std::string &key,T && nullVal=T()) const
        {
            auto it=viewData_.find(key);
            if(it!=viewData_.end())
            {
                try {
                    return any_cast<T>(it->second);
                }
                catch (std::exception& e)
                {
                    LOG_ERROR << e.what();
                }
            }
            return nullVal;
        }
        void insert(const std::string& key,any &&obj)
        {
            viewData_[key]=std::move(obj);
        }
        void insert(const std::string& key,const any &obj)
        {
            viewData_[key]=obj;
        }
        template <typename T>
        void insertAsString(const std::string &key,T && val)
        {
            std::stringstream ss;
            ss<<val;
            viewData_[key] = ss.str();
        }
        void insertFormattedString(const std::string &key,
                                   const char* format, ...)
        {
            std::string strBuffer;
            strBuffer.resize(1024);
            va_list ap,backup_ap;
            va_start(ap, format);
            va_copy(backup_ap, ap);
            auto result=vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
            va_end(backup_ap);
            if ((result >= 0) && ((std::string::size_type)result < strBuffer.size())) {
                strBuffer.resize(result);
            }
            else
            {
                while (true) {
                    if (result < 0) {
                        // Older snprintf() behavior. Just try doubling the buffer size
                        strBuffer.resize(strBuffer.size()*2);
                    } else {
                        strBuffer.resize(result+1);
                    }

                    va_copy(backup_ap, ap);
                    auto result=vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
                    va_end(backup_ap);

                    if ((result >= 0) && ((std::string::size_type)result < strBuffer.size())) {
                        strBuffer.resize(result);
                        break;
                    }
                }
            }
            va_end(ap);
            viewData_[key]=std::move(strBuffer);
        }

        any& operator [] (const std::string &key) const
        {
            return viewData_[key];
        }
    protected:
        mutable ViewDataMap viewData_;
    };
}
