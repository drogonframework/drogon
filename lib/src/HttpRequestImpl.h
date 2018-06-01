// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

//taken from muduo and modified
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

#include <drogon/HttpRequest.h>

#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>

#include <map>
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <string>
using std::string;
using namespace trantor;
namespace drogon
{
    class HttpRequestImpl:public HttpRequest
    {
    public:
        friend class HttpContext;


        HttpRequestImpl()
                : method_(kInvalid),
                  version_(kUnknown),
                  contentLen(0)
        {
        }

        void setVersion(Version v)
        {
            version_ = v;
        }

        Version getVersion() const override
        {
            return version_;
        }
        void parsePremeter();
        bool setMethod(const char* start, const char* end)
        {

            assert(method_ == kInvalid);
            std::string m(start, end);
            if (m == "GET") {
                method_ = kGet;
            } else if (m == "POST") {
                method_ = kPost;
            } else if (m == "HEAD") {
                method_ = kHead;
            } else if (m == "PUT") {
                method_ = kPut;
            } else if (m == "DELETE") {
                method_ = kDelete;
            } else {
                method_ = kInvalid;
            }
            if(method_!=kInvalid)
            {
                content_="";
                query_="";
                cookies_.clear();
                premeter_.clear();
                headers_.clear();
            }
            return method_ != kInvalid;
        }

        bool setMethod(const Method method)
        {
            method_ = method;
            content_="";
            query_="";
            cookies_.clear();
            premeter_.clear();
            headers_.clear();
            return true;
        }

        Method method() const override
        {
            return method_;
        }

        const char* methodString() const override
        {
            const char* result = "UNKNOWN";
            switch(method_) {
                case kGet:
                    result = "GET";
                    break;
                case kPost:
                    result = "POST";
                    break;
                case kHead:
                    result = "HEAD";
                    break;
                case kPut:
                    result = "PUT";
                    break;
                case kDelete:
                    result = "DELETE";
                    break;
                default:
                    break;
            }
            return result;
        }

        void setPath(const char* start, const char* end)
        {
            path_.assign(start, end);
        }
        void setPath(const std::string& path)
        {
            path_ = path;
        }

        std::map<std::string,std::string > getPremeter() const override
        {
            return premeter_;
        }
        const std::string& path() const override
        {
            return path_;
        }

        void setQuery(const char* start, const char* end)
        {
            query_.assign(start, end);
        }
        void setQuery(const std::string& query)
        {
            query_ = query;
        }
//            const string& content() const
//            {
//                return content_;
//            }
        const std::string& query() const override
        {
            if(query_!="")
                return query_;
            if(method_==kPost)
                return content_;
            return query_;
        }

//    void setReceiveTime(Timestamp t)
//    {
//        receiveTime_ = t;
//    }
//
//    Timestamp receiveTime() const
//    {
//        return receiveTime_;
//    }

        void addHeader(const char* start, const char* colon, const char* end)
        {
            std::string field(start, colon);
            ++colon;
            while (colon < end && isspace(*colon)) {
                ++colon;
            }
            std::string value(colon, end);
            while (!value.empty() && isspace(value[value.size() - 1])) {
                value.resize(value.size() - 1);
            }
            headers_[field] = value;
            transform(field.begin(), field.end(), field.begin(), ::tolower);
            if(field == "cookie") {
                LOG_TRACE<<"cookies!!!:"<<value;
                std::string::size_type pos;
                while((pos = value.find(";")) != std::string::npos) {
                    std::string coo = value.substr(0, pos);
                    auto epos = coo.find("=");
                    if(epos != std::string::npos) {
                        std::string cookie_name = coo.substr(0, epos);
                        std::string::size_type cpos=0;
                        while(cpos<cookie_name.length()&&isspace(cookie_name[cpos]))
                            cpos++;
                        cookie_name=cookie_name.substr(cpos);
                        std::string cookie_value = coo.substr(epos + 1);
                        cookies_[cookie_name] = cookie_value;
                    }
                    value=value.substr(pos+1);
                }
                if(value.length()>0)
                {
                    std::string &coo = value;
                    auto epos = coo.find("=");
                    if(epos != std::string::npos) {
                        std::string cookie_name = coo.substr(0, epos);
                        std::string::size_type cpos=0;
                        while(cpos<cookie_name.length()&&isspace(cookie_name[cpos]))
                            cpos++;
                        cookie_name=cookie_name.substr(cpos);
                        std::string cookie_value = coo.substr(epos + 1);
                        cookies_[cookie_name] = cookie_value;
                    }
                }
            }
        }


        std::string getHeader(const std::string& field) const
        {
            std::string result;
            std::map<std::string, std::string>::const_iterator it = headers_.find(field);
            if (it != headers_.end()) {
                result = it->second;
            }
            return result;
        }

        std::string getCookie(const std::string& field) const
        {
            std::string result;
            std::map<std::string, std::string>::const_iterator it = cookies_.find(field);
            if (it != cookies_.end()) {
                result = it->second;
            }
            return result;
        }
        const std::map<std::string, std::string>& headers() const override
        {
            return headers_;
        }

        const std::map<std::string, std::string>& cookies() const override
        {
            return cookies_;
        }

        const std::string& getContent() const
        {
            return content_;
        }
        void swap(HttpRequestImpl& that)
        {
            std::swap(method_, that.method_);
            path_.swap(that.path_);
            query_.swap(that.query_);
            //receiveTime_.swap(that.receiveTime_);
            headers_.swap(that.headers_);
        }

        void setContent(const std::string& content)
        {
            content_ = content;
        }
        void addHeader(const std::string& key, const std::string& value)
        {
            headers_[key] = value;
        }
        void addCookie(const std::string& key, const std::string& value)
        {
            cookies_[key] = value;
        }

        void appendToBuffer(MsgBuffer* output) const;

        virtual SessionPtr session() const override
        {
            return _sessionPtr;
        }

        void setSession(const SessionPtr &session)
        {
            _sessionPtr=session;
        }
    private:
        Method method_;
        Version version_;
        std::string path_;
        std::string query_;

        //trantor::Date receiveTime_;
        std::map<std::string, std::string> headers_;
        std::map<std::string, std::string> cookies_;
        std::map<std::string, std::string> premeter_;

        SessionPtr _sessionPtr;
    protected:
        std::string content_;
        ssize_t contentLen;

    };

}


