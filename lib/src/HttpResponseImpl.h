/**
 *
 *  HttpResponseImpl.h
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
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/MsgBuffer.h>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <unordered_map>

namespace drogon
{
class HttpResponseImpl : public HttpResponse
{
    friend class HttpResponseParser;

  public:
    HttpResponseImpl()
        : _statusCode(kUnknown),
          _creationDate(trantor::Date::now()),
          _closeConnection(false),
          _leftBodyLength(0),
          _currentChunkLength(0),
          _bodyPtr(new std::string())
    {
    }
    HttpResponseImpl(HttpStatusCode code, ContentType type)
        : _statusCode(code),
          _statusMessage(statusCodeToString(code)),
          _creationDate(trantor::Date::now()),
          _closeConnection(false),
          _leftBodyLength(0),
          _currentChunkLength(0),
          _bodyPtr(new std::string()),
          _contentType(type),
          _contentTypeString(webContentTypeToString(type))
    {
    }
    virtual HttpStatusCode statusCode() const override
    {
        return _statusCode;
    }

    virtual const trantor::Date &creationDate() const override
    {
        return _creationDate;
    }

    virtual void setStatusCode(HttpStatusCode code) override
    {
        _statusCode = code;
        setStatusMessage(statusCodeToString(code));
    }

    // virtual void setStatusCode(HttpStatusCode code, const std::string
    // &status_message) override
    // {
    //     _statusCode = code;
    //     setStatusMessage(status_message);
    // }

    virtual void setVersion(const Version v) override
    {
        _v = v;
    }

    virtual void setCloseConnection(bool on) override
    {
        _closeConnection = on;
    }

    virtual bool ifCloseConnection() const override
    {
        return _closeConnection;
    }

    virtual void setContentTypeCode(ContentType type) override
    {
        _contentType = type;
        setContentType(webContentTypeToString(type));
    }

    virtual void setContentTypeCodeAndCustomString(
        ContentType type,
        const char *typeString,
        size_t typeStringLength) override
    {
        _contentType = type;
        setContentType(string_view{typeString, typeStringLength});
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

    virtual const std::string &getHeader(
        const std::string &key,
        const std::string &defaultVal = std::string()) const override
    {
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        return getHeaderBy(field, defaultVal);
    }

    virtual const std::string &getHeader(
        std::string &&key,
        const std::string &defaultVal = std::string()) const override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        return getHeaderBy(key, defaultVal);
    }

    virtual void removeHeader(const std::string &key) override
    {
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        removeHeaderBy(field);
    }

    virtual void removeHeader(std::string &&key) override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        removeHeaderBy(key);
    }

    virtual const std::unordered_map<std::string, std::string> &headers()
        const override
    {
        return _headers;
    }

    const std::string &getHeaderBy(
        const std::string &lowerKey,
        const std::string &defaultVal = std::string()) const
    {
        auto iter = _headers.find(lowerKey);
        if (iter == _headers.end())
        {
            return defaultVal;
        }
        return iter->second;
    }

    void removeHeaderBy(const std::string &lowerKey)
    {
        _headers.erase(lowerKey);
    }

    virtual void addHeader(const std::string &key,
                           const std::string &value) override
    {
        _fullHeaderString.reset();
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        _headers[std::move(field)] = value;
    }

    virtual void addHeader(const std::string &key, std::string &&value) override
    {
        _fullHeaderString.reset();
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        _headers[std::move(field)] = std::move(value);
    }

    void addHeader(const char *start, const char *colon, const char *end);

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        _cookies[key] = Cookie(key, value);
    }

    virtual void addCookie(const Cookie &cookie) override
    {
        _cookies[cookie.key()] = cookie;
    }

    virtual const Cookie &getCookie(
        const std::string &key,
        const Cookie &defaultCookie = Cookie()) const override
    {
        auto it = _cookies.find(key);
        if (it != _cookies.end())
        {
            return it->second;
        }
        return defaultCookie;
    }

    virtual const std::unordered_map<std::string, Cookie> &cookies()
        const override
    {
        return _cookies;
    }

    virtual void removeCookie(const std::string &key) override
    {
        _cookies.erase(key);
    }

    virtual void setBody(const std::string &body) override
    {
        _bodyPtr = std::make_shared<std::string>(body);
    }
    virtual void setBody(std::string &&body) override
    {
        _bodyPtr = std::make_shared<std::string>(std::move(body));
    }

    void redirect(const std::string &url)
    {
        _headers["location"] = url;
    }
    std::shared_ptr<std::string> renderToString() const;
    std::shared_ptr<std::string> renderHeaderForHeadMethod() const;
    virtual void clear() override;

    virtual void setExpiredTime(ssize_t expiredTime) override
    {
        _expriedTime = expiredTime;
    }

    virtual ssize_t expiredTime() const override
    {
        return _expriedTime;
    }

    virtual const std::string &body() const override
    {
        return *_bodyPtr;
    }
    virtual std::string &body() override
    {
        return *_bodyPtr;
    }
    void swap(HttpResponseImpl &that);
    void parseJson() const;
    virtual const std::shared_ptr<Json::Value> jsonObject() const override
    {
        if (!_jsonPtr)
        {
            parseJson();
        }
        return _jsonPtr;
    }
    const std::string &sendfileName() const
    {
        return _sendfileName;
    }
    void setSendfile(const std::string &filename)
    {
        _sendfileName = filename;
    }
    void makeHeaderString()
    {
        _fullHeaderString = std::make_shared<std::string>();
        makeHeaderString(_fullHeaderString);
    }

    void gunzip()
    {
        auto gunzipBody = utils::gzipDecompress(_bodyPtr);
        if (gunzipBody)
        {
            removeHeader("content-encoding");
            _bodyPtr = gunzipBody;
        }
    }
    ~HttpResponseImpl();

  protected:
    void makeHeaderString(
        const std::shared_ptr<std::string> &headerStringPtr) const;

  private:
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, Cookie> _cookies;

    HttpStatusCode _statusCode;
    string_view _statusMessage;

    trantor::Date _creationDate;
    Version _v;
    bool _closeConnection;

    size_t _leftBodyLength;
    size_t _currentChunkLength;
    std::shared_ptr<std::string> _bodyPtr;

    ssize_t _expriedTime = -1;
    std::string _sendfileName;
    mutable std::shared_ptr<Json::Value> _jsonPtr;

    std::shared_ptr<std::string> _fullHeaderString;

    mutable std::shared_ptr<std::string> _httpString;
    mutable std::string::size_type _datePos = std::string::npos;
    mutable int64_t _httpStringDate = -1;

    ContentType _contentType = CT_TEXT_HTML;
    string_view _contentTypeString =
        "Content-Type: text/html; charset=utf-8\r\n";
    void setContentType(const string_view &contentType)
    {
        _contentTypeString = contentType;
    }
    void setStatusMessage(const string_view &message)
    {
        _statusMessage = message;
    }
};
typedef std::shared_ptr<HttpResponseImpl> HttpResponseImplPtr;
}  // namespace drogon
