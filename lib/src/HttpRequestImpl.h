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
#include "CacheFile.h"
#include <drogon/utils/Utilities.h>
#include <drogon/HttpRequest.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/NonCopyable.h>
#include <algorithm>
#include <string>
#include <thread>
#include <unordered_map>
#include <assert.h>
#include <stdio.h>

namespace drogon
{
class HttpRequestImpl : public HttpRequest
{
  public:
    friend class HttpRequestParser;

    explicit HttpRequestImpl(trantor::EventLoop *loop)
        : _method(Invalid),
          _version(kUnknown),
          _date(trantor::Date::now()),
          _contentLen(0),
          _loop(loop)
    {
    }
    void reset()
    {
        _method = Invalid;
        _version = kUnknown;
        _contentLen = 0;
        _headers.clear();
        _cookies.clear();
        _flagForParsingParameters = false;
        _path.clear();
        _matchedPathPattern = "";
        _query.clear();
        _parameters.clear();
        _jsonPtr.reset();
        _sessionPtr.reset();
        _attributesPtr.reset();
        _cacheFilePtr.reset();
        _expect.clear();
        _content.clear();
        _contentType = CT_TEXT_PLAIN;
        _contentTypeString.clear();
        _keepAlive = true;
    }
    trantor::EventLoop *getLoop()
    {
        return _loop;
    }

    void setVersion(Version v)
    {
        _version = v;
        if (v == kHttp10)
        {
            _keepAlive = false;
        }
    }

    virtual Version version() const override
    {
        return _version;
    }

    bool setMethod(const char *start, const char *end);

    virtual void setMethod(const HttpMethod method) override
    {
        _method = method;
        return;
    }

    virtual HttpMethod method() const override
    {
        return _method;
    }

    virtual const char *methodString() const override;

    void setPath(const char *start, const char *end)
    {
        if (utils::needUrlDecoding(start, end))
        {
            _path = utils::urlDecode(start, end);
        }
        else
        {
            _path.append(start, end);
        }
    }

    virtual void setPath(const std::string &path) override
    {
        _path = path;
    }

    virtual const std::unordered_map<std::string, std::string> &parameters()
        const override
    {
        parseParametersOnce();
        return _parameters;
    }

    virtual const std::string &getParameter(
        const std::string &key) const override
    {
        const static std::string defaultVal;
        parseParametersOnce();
        auto iter = _parameters.find(key);
        if (iter != _parameters.end())
            return iter->second;
        return defaultVal;
    }

    virtual const std::string &path() const override
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

    string_view bodyView() const
    {
        if (_cacheFilePtr)
        {
            return _cacheFilePtr->getStringView();
        }
        return _content;
    }
    virtual const char *bodyData() const override
    {
        if (_cacheFilePtr)
        {
            return _cacheFilePtr->getStringView().data();
        }
        return _content.data();
    }
    virtual size_t bodyLength() const override
    {
        if (_cacheFilePtr)
        {
            return _cacheFilePtr->getStringView().length();
        }
        return _content.length();
    }

    void appendToBody(const char *data, size_t length)
    {
        if (_cacheFilePtr)
        {
            _cacheFilePtr->append(data, length);
        }
        else
        {
            _content.append(data, length);
        }
    }

    void reserveBodySize();

    string_view queryView() const
    {
        return _query;
    }

    string_view contentView() const
    {
        if (_cacheFilePtr)
            return _cacheFilePtr->getStringView();
        return _content;
    }

    virtual const std::string &query() const override
    {
        return _query;
    }

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

    void setCreationDate(const trantor::Date &date)
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

    const std::string &getHeader(const std::string &field) const override
    {
        auto lowField = field;
        std::transform(lowField.begin(),
                       lowField.end(),
                       lowField.begin(),
                       tolower);
        return getHeaderBy(lowField);
    }

    const std::string &getHeader(std::string &&field) const override
    {
        std::transform(field.begin(), field.end(), field.begin(), tolower);
        return getHeaderBy(field);
    }

    const std::string &getHeaderBy(const std::string &lowerField) const
    {
        const static std::string defaultVal;
        auto it = _headers.find(lowerField);
        if (it != _headers.end())
        {
            return it->second;
        }
        return defaultVal;
    }

    const std::string &getCookie(const std::string &field) const override
    {
        const static std::string defaultVal;
        auto it = _cookies.find(field);
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

    virtual void setParameter(const std::string &key,
                              const std::string &value) override
    {
        _flagForParsingParameters = true;
        _parameters[key] = value;
    }

    const std::string &getContent() const
    {
        return _content;
    }

    void swap(HttpRequestImpl &that) noexcept;

    void setContent(const std::string &content)
    {
        _content = content;
    }

    virtual void setBody(const std::string &body) override
    {
        _content = body;
    }

    virtual void setBody(std::string &&body) override
    {
        _content = std::move(body);
    }

    virtual void addHeader(const std::string &key,
                           const std::string &value) override
    {
        _headers[key] = value;
    }

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        _cookies[key] = value;
    }

    void appendToBuffer(trantor::MsgBuffer *output) const;

    virtual SessionPtr session() const override
    {
        return _sessionPtr;
    }

    void setSession(const SessionPtr &session)
    {
        _sessionPtr = session;
    }

    virtual AttributesPtr attributes() const override
    {
        if (!_attributesPtr)
        {
            _attributesPtr = std::make_shared<Attributes>();
        }
        return _attributesPtr;
    }

    virtual const std::shared_ptr<Json::Value> jsonObject() const override
    {
        // Not multi-thread safe but good, because we basically call this
        // function in a single thread
        if (!_flagForParsingJson)
        {
            _flagForParsingJson = true;
            parseJson();
        }
        return _jsonPtr;
    }

    virtual void setCustomContentTypeString(const std::string &type) override
    {
        _contentType = CT_NONE;
        _contentTypeString = type;
    }
    virtual void setContentTypeCode(const ContentType type) override
    {
        _contentType = type;
        auto &typeStr = webContentTypeToString(type);
        setContentType(std::string(typeStr.data(), typeStr.length()));
    }

    // virtual void setContentTypeCodeAndCharacterSet(ContentType type, const
    // std::string &charSet = "utf-8") override
    // {
    //     _contentType = type;
    //     setContentType(webContentTypeAndCharsetToString(type, charSet));
    // }

    virtual ContentType contentType() const override
    {
        return _contentType;
    }

    virtual const char *matchedPathPatternData() const override
    {
        return _matchedPathPattern.data();
    }
    virtual size_t matchedPathPatternLength() const override
    {
        return _matchedPathPattern.length();
    }

    void setMatchedPathPattern(const std::string &pathPattern)
    {
        _matchedPathPattern = pathPattern;
    }
    const std::string &expect() const
    {
        return _expect;
    }
    bool keepAlive() const
    {
        return _keepAlive;
    }
    ~HttpRequestImpl();

  protected:
    friend class HttpRequest;
    void setContentType(const std::string &contentType)
    {
        _contentTypeString = contentType;
    }
    void setContentType(std::string &&contentType)
    {
        _contentTypeString = std::move(contentType);
    }

  private:
    void parseParameters() const;
    void parseParametersOnce() const
    {
        // Not multi-thread safe but good, because we basically call this
        // function in a single thread
        if (!_flagForParsingParameters)
        {
            _flagForParsingParameters = true;
            parseParameters();
        }
    }

    void parseJson() const;
    mutable bool _flagForParsingParameters = false;
    mutable bool _flagForParsingJson = false;
    HttpMethod _method;
    Version _version;
    std::string _path;
    string_view _matchedPathPattern = "";
    std::string _query;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _cookies;
    mutable std::unordered_map<std::string, std::string> _parameters;
    mutable std::shared_ptr<Json::Value> _jsonPtr;
    SessionPtr _sessionPtr;
    mutable AttributesPtr _attributesPtr;
    trantor::InetAddress _peer;
    trantor::InetAddress _local;
    trantor::Date _date;
    std::unique_ptr<CacheFile> _cacheFilePtr;
    std::string _expect;
    bool _keepAlive = true;

  protected:
    std::string _content;
    size_t _contentLen;
    trantor::EventLoop *_loop;
    ContentType _contentType = CT_TEXT_PLAIN;
    std::string _contentTypeString;
};

typedef std::shared_ptr<HttpRequestImpl> HttpRequestImplPtr;

inline void swap(HttpRequestImpl &one, HttpRequestImpl &two) noexcept
{
    one.swap(two);
}

}  // namespace drogon
