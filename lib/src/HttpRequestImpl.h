/**
 *
 *  HttpRequestImpl.h
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

#include "HttpUtils.h"
#include <drogon/utils/Utilities.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/net/InetAddress.h>
#include <unordered_map>
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
    friend class HttpRequestParser;

    HttpRequestImpl()
        : _method(Invalid),
          _version(kUnknown),
          _date(trantor::Date::now()),
          _contentLen(0)
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

    void parseParameter();

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
        // if (_method != Invalid)
        // {
        //     _content = "";
        //     _query = "";
        //     _cookies.clear();
        //     _parameters.clear();
        //     _headers.clear();
        // }
        return _method != Invalid;
    }

    virtual void setMethod(const HttpMethod method) override
    {
        _method = method;
        // _content = "";
        // _query = "";
        // _cookies.clear();
        // _parameters.clear();
        // _headers.clear();
        return;
    }

    HttpMethod method() const override
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
        _path = urlDecode(start, end);
    }

    virtual void setPath(const std::string &path) override
    {
        _path = path;
    }

    const std::unordered_map<std::string, std::string> &getParameters() const override
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
    //                return _content;
    //            }
    const std::string &query() const override
    {
        if (_query != "")
            return _query;
        if (_method == Post)
            return _content;
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

    virtual const trantor::Date &creationDate() const override
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

    void addHeader(const char *start, const char *colon, const char *end);

    const std::string &getHeader(const std::string &field, const std::string &defaultVal = std::string()) const override
    {
        auto lowField = field;
        std::transform(lowField.begin(), lowField.end(), lowField.begin(), tolower);
        return getHeaderBy(lowField, defaultVal);
    }

    const std::string &getHeader(std::string &&field, const std::string &defaultVal = std::string()) const override
    {
        std::transform(field.begin(), field.end(), field.begin(), tolower);
        return getHeaderBy(field, defaultVal);
    }

    const std::string &getHeaderBy(const std::string &lowerField, const std::string &defaultVal = std::string()) const
    {
        auto it = _headers.find(lowerField);
        if (it != _headers.end())
        {
            return it->second;
        }
        return defaultVal;
    }

    const std::string &getCookie(const std::string &field, const std::string &defaultVal = std::string()) const override
    {
        std::unordered_map<std::string, std::string>::const_iterator it = _cookies.find(field);
        if (it != _cookies.end())
        {
            return it->second;
        }
        return defaultVal;
    }

    const std::unordered_map<std::string, std::string> &headers() const override
    {
        return _headers;
    }

    const std::unordered_map<std::string, std::string> &cookies() const override
    {
        return _cookies;
    }

    virtual void setParameter(const std::string &key, const std::string &value) override
    {
        _parameters[key] = value;
    }

    const std::string &getContent() const
    {
        return _content;
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
        _content.swap(that._content);
        std::swap(_contentLen, that._contentLen);
    }

    void setContent(const std::string &content)
    {
        _content = content;
    }

    virtual void addHeader(const std::string &key, const std::string &value) override
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

    virtual void setContentTypeCode(ContentType type) override
    {
        _contentType = type;
        setContentType(webContentTypeToString(type));
    }

    virtual void setContentTypeCodeAndCharacterSet(ContentType type, const std::string &charSet = "utf-8") override
    {
        _contentType = type;
        setContentType(webContentTypeAndCharsetToString(type, charSet));
    }

    virtual ContentType getContentTypeCode() override
    {
        return _contentType;
    }

  protected:
    friend class HttpRequest;
    void setContentType(const std::string &contentType)
    {
        addHeader("Content-Type", contentType);
    }
    void setContentType(std::string &&contentType)
    {
        addHeader("Content-Type", std::move(contentType));
    }

  private:
    HttpMethod _method;
    Version _version;
    std::string _path;
    std::string _query;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _cookies;
    std::unordered_map<std::string, std::string> _parameters;
    std::shared_ptr<Json::Value> _jsonPtr;
    SessionPtr _sessionPtr;
    trantor::InetAddress _peer;
    trantor::InetAddress _local;
    trantor::Date _date;
    
  protected:
    std::string _content;
    size_t _contentLen;
    ContentType _contentType = CT_TEXT_PLAIN;
};

typedef std::shared_ptr<HttpRequestImpl> HttpRequestImplPtr;

} // namespace drogon
