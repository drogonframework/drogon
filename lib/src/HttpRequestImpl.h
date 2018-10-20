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

#include <drogon/utils/Utilities.h>
#include <drogon/HttpRequest.h>

#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/net/InetAddress.h>
#include <map>
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <string>
using std::string;
using namespace trantor;
namespace drogon
{
class HttpRequestImpl : public HttpRequest
{
  public:
    friend class HttpContext;

    HttpRequestImpl()
        : _method(Invalid),
          _version(kUnknown),
          contentLen(0)
    {
    }

    void setVersion(Version v)
    {
        _version = v;
    }

    Version getVersion() const override
    {
        return _version;
    }
    void parsePremeter();
    bool setMethod(const char *start, const char *end)
    {

        assert(_method == Invalid);
        std::string m(start, end);
        if (m == "GET")
        {
            _method = Get;
        }
        else if (m == "POST")
        {
            _method = Post;
        }
        else if (m == "HEAD")
        {
            _method = Head;
        }
        else if (m == "PUT")
        {
            _method = Put;
        }
        else if (m == "DELETE")
        {
            _method = Delete;
        }
        else
        {
            _method = Invalid;
        }
        if (_method != Invalid)
        {
            content_ = "";
            _query = "";
            _cookies.clear();
            _parameters.clear();
            _headers.clear();
        }
        return _method != Invalid;
    }

    virtual void setMethod(const Method method) override
    {
        _method = method;
        content_ = "";
        _query = "";
        _cookies.clear();
        _parameters.clear();
        _headers.clear();
        return;
    }

    Method method() const override
    {
        return _method;
    }

    const char *methodString() const override
    {
        const char *result = "UNKNOWN";
        switch (_method)
        {
        case Get:
            result = "GET";
            break;
        case Post:
            result = "POST";
            break;
        case Head:
            result = "HEAD";
            break;
        case Put:
            result = "PUT";
            break;
        case Delete:
            result = "DELETE";
            break;
        default:
            break;
        }
        return result;
    }

    void setPath(const char *start, const char *end)
    {
        _path = urlDecode(std::string(start, end));
    }
    virtual void setPath(const std::string &path) override
    {
        _path = path;
    }

    const std::map<std::string, std::string> &getParameters() const override
    {
        return _parameters;
    }
    const std::string &path() const override
    {
        return _path;
    }

    void setQuery(const char *start, const char *end)
    {
        _query.assign(start, end);
    }
    void setQuery(const std::string &query)
    {
        _query = query;
    }
    //            const string& content() const
    //            {
    //                return content_;
    //            }
    const std::string &query() const override
    {
        if (_query != "")
            return _query;
        if (_method == Post)
            return content_;
        return _query;
    }

    //
    //    Timestamp receiveTime() const
    //    {
    //        return receiveTime_;
    //    }

    virtual const trantor::InetAddress &peerAddr() const override
    {
        return _peer;
    }
    virtual const trantor::InetAddress &localAddr() const override
    {
        return _local;
    }
    virtual const trantor::Date &receiveDate() const override
    {
        return _date;
    }
    void setReceiveDate(const trantor::Date &date)
    {
        _date = date;
    }
    void setPeerAddr(const trantor::InetAddress &peer)
    {
        _peer = peer;
    }
    void setLocalAddr(const trantor::InetAddress &local)
    {
        _local = local;
    }

    void addHeader(const char *start, const char *colon, const char *end)
    {
        std::string field(start, colon);
        //field name is case-insensitive.so we transform it to lower;(rfc2616-4.2)
        std::transform(field.begin(), field.end(), field.begin(), ::tolower);
        ++colon;
        while (colon < end && isspace(*colon))
        {
            ++colon;
        }
        std::string value(colon, end);
        while (!value.empty() && isspace(value[value.size() - 1]))
        {
            value.resize(value.size() - 1);
        }
        _headers[field] = value;
        if (field == "cookie")
        {
            LOG_TRACE << "cookies!!!:" << value;
            std::string::size_type pos;
            while ((pos = value.find(";")) != std::string::npos)
            {
                std::string coo = value.substr(0, pos);
                auto epos = coo.find("=");
                if (epos != std::string::npos)
                {
                    std::string cookie_name = coo.substr(0, epos);
                    std::string::size_type cpos = 0;
                    while (cpos < cookie_name.length() && isspace(cookie_name[cpos]))
                        cpos++;
                    cookie_name = cookie_name.substr(cpos);
                    std::string cookie_value = coo.substr(epos + 1);
                    _cookies[cookie_name] = cookie_value;
                }
                value = value.substr(pos + 1);
            }
            if (value.length() > 0)
            {
                std::string &coo = value;
                auto epos = coo.find("=");
                if (epos != std::string::npos)
                {
                    std::string cookie_name = coo.substr(0, epos);
                    std::string::size_type cpos = 0;
                    while (cpos < cookie_name.length() && isspace(cookie_name[cpos]))
                        cpos++;
                    cookie_name = cookie_name.substr(cpos);
                    std::string cookie_value = coo.substr(epos + 1);
                    _cookies[cookie_name] = cookie_value;
                }
            }
        }
    }

    std::string getHeader(const std::string &field) const override
    {
        std::string result;
        auto lowField = field;
        std::transform(lowField.begin(), lowField.end(), lowField.begin(), tolower);
        std::map<std::string, std::string>::const_iterator it = _headers.find(lowField);
        if (it != _headers.end())
        {
            result = it->second;
        }
        return result;
    }

    std::string getCookie(const std::string &field) const override
    {
        std::string result;
        std::map<std::string, std::string>::const_iterator it = _cookies.find(field);
        if (it != _cookies.end())
        {
            result = it->second;
        }
        return result;
    }
    const std::map<std::string, std::string> &headers() const override
    {
        return _headers;
    }

    const std::map<std::string, std::string> &cookies() const override
    {
        return _cookies;
    }
    virtual void setParameter(const std::string &key, const std::string &value) override
    {
        _parameters[key] = value;
    }
    const std::string &getContent() const
    {
        return content_;
    }
    void swap(HttpRequestImpl &that)
    {
        std::swap(_method, that._method);
        std::swap(_version, that._version);
        _path.swap(that._path);
        _query.swap(that._query);

        _headers.swap(that._headers);
        _cookies.swap(that._cookies);
        _parameters.swap(that._parameters);
        _jsonPtr.swap(that._jsonPtr);
        _sessionPtr.swap(that._sessionPtr);

        std::swap(_peer, that._peer);
        std::swap(_local, that._local);
        _date.swap(that._date);
        content_.swap(that.content_);
        std::swap(contentLen, that.contentLen);
    }

    void setContent(const std::string &content)
    {
        content_ = content;
    }
    void addHeader(const std::string &key, const std::string &value)
    {
        _headers[key] = value;
    }
    void addCookie(const std::string &key, const std::string &value)
    {
        _cookies[key] = value;
    }

    void appendToBuffer(MsgBuffer *output) const;

    virtual SessionPtr session() const override
    {
        return _sessionPtr;
    }

    void setSession(const SessionPtr &session)
    {
        _sessionPtr = session;
    }

    virtual const std::shared_ptr<Json::Value> getJsonObject() const override
    {
        return _jsonPtr;
    }

  private:
    Method _method = Get;
    Version _version = kHttp11;
    std::string _path;
    std::string _query;

    //trantor::Date receiveTime_;
    std::map<std::string, std::string> _headers;
    std::map<std::string, std::string> _cookies;
    std::map<std::string, std::string> _parameters;
    std::shared_ptr<Json::Value> _jsonPtr;
    SessionPtr _sessionPtr;
    trantor::InetAddress _peer;
    trantor::InetAddress _local;
    trantor::Date _date;

  protected:
    std::string content_;
    size_t contentLen;
};

typedef std::shared_ptr<HttpRequestImpl> HttpRequestImplPtr;
} // namespace drogon
