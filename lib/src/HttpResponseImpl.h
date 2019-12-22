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
    HttpResponseImpl() : creationDate_(trantor::Date::now())
    {
    }
    HttpResponseImpl(HttpStatusCode code, ContentType type)
        : statusCode_(code),
          statusMessage_(statusCodeToString(code)),
          creationDate_(trantor::Date::now()),
          contentType_(type),
          contentTypeString_(webContentTypeToString(type))
    {
    }
    virtual void setPassThrough(bool flag) override
    {
        passThrough_ = flag;
    }
    virtual HttpStatusCode statusCode() const override
    {
        return statusCode_;
    }

    virtual const trantor::Date &creationDate() const override
    {
        return creationDate_;
    }

    virtual void setStatusCode(HttpStatusCode code) override
    {
        statusCode_ = code;
        setStatusMessage(statusCodeToString(code));
    }

    virtual void setVersion(const Version v) override
    {
        version_ = v;
        if (version_ == Version::kHttp10)
        {
            closeConnection_ = true;
        }
    }

    virtual Version version() const override
    {
        return version_;
    }

    virtual void setCloseConnection(bool on) override
    {
        closeConnection_ = on;
    }

    virtual bool ifCloseConnection() const override
    {
        return closeConnection_;
    }

    virtual void setContentTypeCode(ContentType type) override
    {
        contentType_ = type;
        setContentType(webContentTypeToString(type));
    }

    virtual void setContentTypeCodeAndCustomString(
        ContentType type,
        const char *typeString,
        size_t typeStringLength) override
    {
        contentType_ = type;
        setContentType(string_view{typeString, typeStringLength});
    }

    // virtual void setContentTypeCodeAndCharacterSet(ContentType type, const
    // std::string &charSet = "utf-8") override
    // {
    //     contentType_ = type;
    //     setContentType(webContentTypeAndCharsetToString(type, charSet));
    // }

    virtual ContentType contentType() const override
    {
        return contentType_;
    }

    virtual const std::string &getHeader(const std::string &key) const override
    {
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        return getHeaderBy(field);
    }

    virtual const std::string &getHeader(std::string &&key) const override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        return getHeaderBy(key);
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
        return headers_;
    }

    const std::string &getHeaderBy(const std::string &lowerKey) const
    {
        const static std::string defaultVal;
        auto iter = headers_.find(lowerKey);
        if (iter == headers_.end())
        {
            return defaultVal;
        }
        return iter->second;
    }

    void removeHeaderBy(const std::string &lowerKey)
    {
        headers_.erase(lowerKey);
    }

    virtual void addHeader(const std::string &key,
                           const std::string &value) override
    {
        fullHeaderString_.reset();
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        headers_[std::move(field)] = value;
    }

    virtual void addHeader(const std::string &key, std::string &&value) override
    {
        fullHeaderString_.reset();
        auto field = key;
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        headers_[std::move(field)] = std::move(value);
    }

    void addHeader(const char *start, const char *colon, const char *end);

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        cookies_[key] = Cookie(key, value);
    }

    virtual void addCookie(const Cookie &cookie) override
    {
        cookies_[cookie.key()] = cookie;
    }

    virtual void addCookie(Cookie &&cookie) override
    {
        cookies_[cookie.key()] = std::move(cookie);
    }

    virtual const Cookie &getCookie(const std::string &key) const override
    {
        static const Cookie defaultCookie;
        auto it = cookies_.find(key);
        if (it != cookies_.end())
        {
            return it->second;
        }
        return defaultCookie;
    }

    virtual const std::unordered_map<std::string, Cookie> &cookies()
        const override
    {
        return cookies_;
    }

    virtual void removeCookie(const std::string &key) override
    {
        cookies_.erase(key);
    }

    virtual void setBody(const std::string &body) override
    {
        bodyPtr_ = std::make_shared<std::string>(body);
        bodyViewPtr_.reset();
    }
    virtual void setBody(std::string &&body) override
    {
        bodyPtr_ = std::make_shared<std::string>(std::move(body));
        bodyViewPtr_.reset();
    }

    void redirect(const std::string &url)
    {
        headers_["location"] = url;
    }
    std::shared_ptr<std::string> renderToString();
    void renderToBuffer(trantor::MsgBuffer &buffer);
    std::shared_ptr<std::string> renderHeaderForHeadMethod();
    virtual void clear() override;

    virtual void setExpiredTime(ssize_t expiredTime) override
    {
        expriedTime_ = expiredTime;
        datePos_ = std::string::npos;
    }

    virtual ssize_t expiredTime() const override
    {
        return expriedTime_;
    }

    virtual const std::string &body() const override
    {
        if (!bodyPtr_)
        {
            if (bodyViewPtr_)
            {
                bodyPtr_ =
                    std::make_shared<std::string>(bodyViewPtr_->data(),
                                                  bodyViewPtr_->length());
            }
            else
            {
                bodyPtr_ = std::make_shared<std::string>();
            }
        }
        return *bodyPtr_;
    }
    virtual std::string &body() override
    {
        if (!bodyPtr_)
        {
            if (bodyViewPtr_)
            {
                bodyPtr_ =
                    std::make_shared<std::string>(bodyViewPtr_->data(),
                                                  bodyViewPtr_->length());
            }
            else
            {
                bodyPtr_ = std::make_shared<std::string>();
            }
        }
        return *bodyPtr_;
    }
    void swap(HttpResponseImpl &that) noexcept;
    void parseJson() const;
    virtual const std::shared_ptr<Json::Value> jsonObject() const override
    {
        // Not multi-thread safe but good, because we basically call this
        // function in a single thread
        if (!flagForParsingJson_)
        {
            flagForParsingJson_ = true;
            parseJson();
        }
        return jsonPtr_;
    }
    void setJsonObject(const Json::Value &pJson)
    {
        jsonPtr_ = std::make_shared<Json::Value>(pJson);
    }
    void setJsonObject(Json::Value &&pJson)
    {
        jsonPtr_ = std::make_shared<Json::Value>(std::move(pJson));
    }
    void generateBodyFromJson();
    const std::string &sendfileName() const
    {
        return sendfileName_;
    }
    void setSendfile(const std::string &filename)
    {
        sendfileName_ = filename;
    }
    void makeHeaderString()
    {
        fullHeaderString_ = std::make_shared<std::string>();
        makeHeaderString(fullHeaderString_);
    }

    void gunzip()
    {
        if (bodyPtr_)
        {
            auto gunzipBody =
                utils::gzipDecompress(bodyPtr_->data(), bodyPtr_->length());
            removeHeader("content-encoding");
            bodyPtr_ = std::make_shared<std::string>(move(gunzipBody));
        }
        else if (bodyViewPtr_)
        {
            auto gunzipBody = utils::gzipDecompress(bodyViewPtr_->data(),
                                                    bodyViewPtr_->length());
            removeHeader("content-encoding");
            bodyPtr_ = std::make_shared<std::string>(move(gunzipBody));
        }
    }
    ~HttpResponseImpl();

  protected:
    void makeHeaderString(const std::shared_ptr<std::string> &headerStringPtr);

  private:
    virtual void setBody(const char *body, size_t len) override
    {
        bodyViewPtr_ = std::make_shared<string_view>(body, len);
        bodyPtr_.reset();
    }
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, Cookie> cookies_;

    HttpStatusCode statusCode_{kUnknown};
    string_view statusMessage_;

    trantor::Date creationDate_;
    Version version_{Version::kHttp11};
    bool closeConnection_{false};

    size_t leftBodyLength_{0};
    size_t currentChunkLength_{0};
    mutable std::shared_ptr<std::string> bodyPtr_;
    std::shared_ptr<string_view> bodyViewPtr_;
    ssize_t expriedTime_{-1};
    std::string sendfileName_;
    mutable std::shared_ptr<Json::Value> jsonPtr_;

    std::shared_ptr<std::string> fullHeaderString_;

    mutable std::shared_ptr<std::string> httpString_;
    mutable std::string::size_type datePos_{std::string::npos};
    mutable int64_t httpStringDate_{-1};
    mutable bool flagForParsingJson_{false};
    ContentType contentType_{CT_TEXT_HTML};
    string_view contentTypeString_{
        "Content-Type: text/html; charset=utf-8\r\n"};
    bool passThrough_{false};
    void setContentType(const string_view &contentType)
    {
        contentTypeString_ = contentType;
    }
    void setStatusMessage(const string_view &message)
    {
        statusMessage_ = message;
    }
};
using HttpResponseImplPtr = std::shared_ptr<HttpResponseImpl>;

inline void swap(HttpResponseImpl &one, HttpResponseImpl &two) noexcept
{
    one.swap(two);
}

}  // namespace drogon
