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
#include <trantor/utils/MsgBuffer.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <drogon/utils/Utilities.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

using namespace trantor;
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

    // virtual void setStatusCode(HttpStatusCode code, const std::string &status_message) override
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

    // virtual void setContentTypeCodeAndCharacterSet(ContentType type, const std::string &charSet = "utf-8") override
    // {
    //     _contentType = type;
    //     setContentType(webContentTypeAndCharsetToString(type, charSet));
    // }

    virtual ContentType contentType() const override
    {
        return _contentType;
    }

    virtual const std::string &getHeader(const std::string &key, const std::string &defaultVal = std::string()) const override
    {
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        return getHeaderBy(field, defaultVal);
    }

    virtual const std::string &getHeader(std::string &&key, const std::string &defaultVal = std::string()) const override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        return getHeaderBy(key, defaultVal);
    }

    virtual const std::unordered_map<std::string, std::string> &headers() const override
    {
        return _headers;
    }

    const std::string &getHeaderBy(const std::string &lowerKey, const std::string &defaultVal = std::string()) const
    {
        auto iter = _headers.find(lowerKey);
        if (iter == _headers.end())
        {
            return defaultVal;
        }
        return iter->second;
    }

    virtual void addHeader(const std::string &key, const std::string &value) override
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

    void addHeader(const char *start, const char *colon, const char *end)
    {
        _fullHeaderString.reset();
        std::string field(start, colon);
        transform(field.begin(), field.end(), field.begin(), ::tolower);
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

        if (field == "set-cookie")
        {
            //LOG_INFO<<"cookies!!!:"<<value;
            auto values = utils::splitString(value, ";");
            Cookie cookie;
            cookie.setHttpOnly(false);
            for (size_t i = 0; i < values.size(); i++)
            {
                std::string &coo = values[i];
                std::string cookie_name;
                std::string cookie_value;
                auto epos = coo.find("=");
                if (epos != std::string::npos)
                {
                    cookie_name = coo.substr(0, epos);
                    std::string::size_type cpos = 0;
                    while (cpos < cookie_name.length() && isspace(cookie_name[cpos]))
                        cpos++;
                    cookie_name = cookie_name.substr(cpos);
                    cookie_value = coo.substr(epos + 1);
                }
                else
                {
                    std::string::size_type cpos = 0;
                    while (cpos < coo.length() && isspace(coo[cpos]))
                        cpos++;
                    cookie_name = coo.substr(cpos);
                }
                if (i == 0)
                {
                    cookie.setKey(cookie_name);
                    cookie.setValue(cookie_value);
                }
                else
                {
                    std::transform(cookie_name.begin(), cookie_name.end(), cookie_name.begin(), tolower);
                    if (cookie_name == "path")
                    {
                        cookie.setPath(cookie_value);
                    }
                    else if (cookie_name == "domain")
                    {
                        cookie.setDomain(cookie_value);
                    }
                    else if (cookie_name == "expires")
                    {
                        cookie.setExpiresDate(utils::getHttpDate(cookie_value));
                    }
                    else if (cookie_name == "secure")
                    {
                        cookie.setSecure(true);
                    }
                    else if (cookie_name == "httponly")
                    {
                        cookie.setHttpOnly(true);
                    }
                }
            }
            if (!cookie.key().empty())
            {
                _cookies[cookie.key()] = cookie;
            }
        }
        else
        {
            _headers[std::move(field)] = std::move(value);
        }
    }

    virtual void addCookie(const std::string &key, const std::string &value) override
    {
        _cookies[key] = Cookie(key, value);
    }

    virtual void addCookie(const Cookie &cookie) override
    {
        _cookies[cookie.key()] = cookie;
    }

    virtual const Cookie &getCookie(const std::string &key, const Cookie &defaultCookie = Cookie()) const override
    {
        auto it = _cookies.find(key);
        if (it != _cookies.end())
        {
            return it->second;
        }
        return defaultCookie;
    }

    virtual const std::unordered_map<std::string, Cookie> &cookies() const override
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
    virtual void clear() override
    {
        _statusCode = kUnknown;
        _v = kHttp11;
        _statusMessage = nullptr;
        _fullHeaderString.reset();
        _headers.clear();
        _cookies.clear();
        _bodyPtr.reset(new std::string());
        _leftBodyLength = 0;
        _currentChunkLength = 0;
        _jsonPtr.reset();
    }

    virtual void setExpiredTime(ssize_t expiredTime) override
    {
        _expriedTime = expiredTime;
    }

    virtual ssize_t expiredTime() const override { return _expriedTime; }

    //	void setReceiveTime(trantor::Date t)
    //    {
    //        receiveTime_ = t;
    //    }

    virtual const std::string &body() const override
    {
        return *_bodyPtr;
    }
    virtual std::string &body() override
    {
        return *_bodyPtr;
    }
    void swap(HttpResponseImpl &that)
    {
        using std::swap;
        _headers.swap(that._headers);
        _cookies.swap(that._cookies);
        swap(_statusCode, that._statusCode);
        swap(_v, that._v);
        swap(_statusMessage, that._statusMessage);
        swap(_closeConnection, that._closeConnection);
        _bodyPtr.swap(that._bodyPtr);
        swap(_leftBodyLength, that._leftBodyLength);
        swap(_currentChunkLength, that._currentChunkLength);
        swap(_contentType, that._contentType);
        _jsonPtr.swap(that._jsonPtr);
        _fullHeaderString.swap(that._fullHeaderString);
        _httpString.swap(that._httpString);
        swap(_datePos, that._datePos);
    }
    void parseJson() const
    {
        //parse json data in reponse
        _jsonPtr = std::make_shared<Json::Value>();
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(_bodyPtr->data(), _bodyPtr->data() + _bodyPtr->size(), _jsonPtr.get(), &errs))
        {
            LOG_ERROR << errs;
            _jsonPtr.reset();
        }
    }
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
            _bodyPtr = gunzipBody;
    }

protected:
    void makeHeaderString(const std::shared_ptr<std::string> &headerStringPtr) const;

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
    string_view _contentTypeString = "Content-Type: text/html; charset=utf-8\r\n";
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
} // namespace drogon
