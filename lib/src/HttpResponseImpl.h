/**
 *
 *  @file HttpResponseImpl.h
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

#include "HttpUtils.h"
#include "HttpMessageBody.h"
#include <drogon/exports.h>
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
class DROGON_EXPORT HttpResponseImpl : public HttpResponse
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
          flagForParsingContentType_(true),
          contentTypeString_(contentTypeToMime(type))
    {
    }
    void setPassThrough(bool flag) override
    {
        passThrough_ = flag;
    }
    HttpStatusCode statusCode() const override
    {
        return statusCode_;
    }

    const trantor::Date &creationDate() const override
    {
        return creationDate_;
    }

    void setStatusCode(HttpStatusCode code) override
    {
        statusCode_ = code;
        setStatusMessage(statusCodeToString(code));
    }

    void setVersion(const Version v) override
    {
        version_ = v;
        if (version_ == Version::kHttp10)
        {
            closeConnection_ = true;
        }
    }

    Version version() const override
    {
        return version_;
    }

    virtual const char *versionString() const override;

    void setCloseConnection(bool on) override
    {
        closeConnection_ = on;
    }

    bool ifCloseConnection() const override
    {
        return closeConnection_;
    }

    void setContentTypeCode(ContentType type) override
    {
        contentType_ = type;
        auto ct = contentTypeToMime(type);
        contentTypeString_ = std::string(ct.data(), ct.size());
        flagForParsingContentType_ = true;
    }

    //  void setContentTypeCodeAndCharacterSet(ContentType type, const
    // std::string &charSet = "utf-8") override
    // {
    //     contentType_ = type;
    //     setContentType(webContentTypeAndCharsetToString(type, charSet));
    // }

    ContentType contentType() const override
    {
        parseContentTypeAndString();
        return contentType_;
    }

    const std::string &getHeader(std::string key) const override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        return getHeaderBy(key);
    }

    void removeHeader(std::string key) override
    {
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        removeHeaderBy(key);
    }

    const std::unordered_map<std::string, std::string> &headers() const override
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
        fullHeaderString_.reset();
        headers_.erase(lowerKey);
    }

    void addHeader(std::string field, const std::string &value) override
    {
        fullHeaderString_.reset();
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        headers_[std::move(field)] = value;
    }

    void addHeader(std::string field, std::string &&value) override
    {
        fullHeaderString_.reset();
        transform(field.begin(), field.end(), field.begin(), ::tolower);
        headers_[std::move(field)] = std::move(value);
    }

    void addHeader(const char *start, const char *colon, const char *end);

    void addCookie(const std::string &key, const std::string &value) override
    {
        cookies_[key] = Cookie(key, value);
    }

    void addCookie(const Cookie &cookie) override
    {
        cookies_[cookie.key()] = cookie;
    }

    void addCookie(Cookie &&cookie) override
    {
        cookies_[cookie.key()] = std::move(cookie);
    }

    const Cookie &getCookie(const std::string &key) const override
    {
        static const Cookie defaultCookie;
        auto it = cookies_.find(key);
        if (it != cookies_.end())
        {
            return it->second;
        }
        return defaultCookie;
    }

    const std::unordered_map<std::string, Cookie> &cookies() const override
    {
        return cookies_;
    }

    void removeCookie(const std::string &key) override
    {
        cookies_.erase(key);
    }

    void setBody(const std::string &body) override
    {
        bodyPtr_ = std::make_shared<HttpMessageStringBody>(body);
        if (passThrough_)
        {
            addHeader("content-length", std::to_string(bodyPtr_->length()));
        }
    }
    void setBody(std::string &&body) override
    {
        bodyPtr_ = std::make_shared<HttpMessageStringBody>(std::move(body));
        if (passThrough_)
        {
            addHeader("content-length", std::to_string(bodyPtr_->length()));
        }
    }

    void redirect(const std::string &url)
    {
        headers_["location"] = url;
    }
    std::shared_ptr<trantor::MsgBuffer> renderToBuffer();
    void renderToBuffer(trantor::MsgBuffer &buffer);
    std::shared_ptr<trantor::MsgBuffer> renderHeaderForHeadMethod();
    void clear() override;

    void setExpiredTime(ssize_t expiredTime) override
    {
        expriedTime_ = expiredTime;
        datePos_ = std::string::npos;
        if (expriedTime_ < 0 && version_ == Version::kHttp10)
        {
            fullHeaderString_.reset();
        }
    }

    ssize_t expiredTime() const override
    {
        return expriedTime_;
    }

    const char *getBodyData() const override
    {
        if (!flagForSerializingJson_ && jsonPtr_)
        {
            generateBodyFromJson();
        }
        else if (!bodyPtr_)
        {
            return nullptr;
        }
        return bodyPtr_->data();
    }
    size_t getBodyLength() const override
    {
        if (bodyPtr_)
            return bodyPtr_->length();
        return 0;
    }

    void swap(HttpResponseImpl &that) noexcept;
    void parseJson() const;
    const std::shared_ptr<Json::Value> &jsonObject() const override
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
    const std::string &getJsonError() const override
    {
        const static std::string none;
        if (jsonParsingErrorPtr_)
            return *jsonParsingErrorPtr_;
        return none;
    }
    void setJsonObject(const Json::Value &pJson)
    {
        flagForParsingJson_ = true;
        flagForSerializingJson_ = false;
        jsonPtr_ = std::make_shared<Json::Value>(pJson);
    }
    void setJsonObject(Json::Value &&pJson)
    {
        flagForParsingJson_ = true;
        flagForSerializingJson_ = false;
        jsonPtr_ = std::make_shared<Json::Value>(std::move(pJson));
    }
    bool shouldBeCompressed() const;
    void generateBodyFromJson() const;
    const std::string &sendfileName() const override
    {
        return sendfileName_;
    }
    const SendfileRange &sendfileRange() const override
    {
        return sendfileRange_;
    }
    void setSendfile(const std::string &filename)
    {
        sendfileName_ = filename;
    }
    void setSendfileRange(size_t offset, size_t len)
    {
        sendfileRange_.first = offset;
        sendfileRange_.second = len;
    }
    void makeHeaderString()
    {
        fullHeaderString_ = std::make_shared<trantor::MsgBuffer>(128);
        makeHeaderString(*fullHeaderString_);
    }

    std::string contentTypeString() const override
    {
        parseContentTypeAndString();
        return contentTypeString_;
    }

    void gunzip()
    {
        if (bodyPtr_)
        {
            auto gunzipBody =
                utils::gzipDecompress(bodyPtr_->data(), bodyPtr_->length());
            removeHeaderBy("content-encoding");
            bodyPtr_ =
                std::make_shared<HttpMessageStringBody>(move(gunzipBody));
            addHeader("content-length", std::to_string(bodyPtr_->length()));
        }
    }
#ifdef USE_BROTLI
    void brDecompress()
    {
        if (bodyPtr_)
        {
            auto gunzipBody =
                utils::brotliDecompress(bodyPtr_->data(), bodyPtr_->length());
            removeHeaderBy("content-encoding");
            bodyPtr_ =
                std::make_shared<HttpMessageStringBody>(move(gunzipBody));
            addHeader("content-length", std::to_string(bodyPtr_->length()));
        }
    }
#endif
    ~HttpResponseImpl() override = default;

  protected:
    void makeHeaderString(trantor::MsgBuffer &headerString);

    void parseContentTypeAndString() const
    {
        if (!flagForParsingContentType_)
        {
            flagForParsingContentType_ = true;
            auto &contentTypeString = getHeaderBy("content-type");
            if (contentTypeString == "")
            {
                contentType_ = CT_NONE;
            }
            else
            {
                auto pos = contentTypeString.find(';');
                if (pos != std::string::npos)
                {
                    contentType_ = parseContentType(
                        string_view(contentTypeString.data(), pos));
                }
                else
                {
                    contentType_ =
                        parseContentType(string_view(contentTypeString));
                }

                if (contentType_ == CT_NONE)
                    contentType_ = CT_CUSTOM;
                contentTypeString_ = contentTypeString;
            }
        }
    }

  private:
    void setBody(const char *body, size_t len) override
    {
        bodyPtr_ = std::make_shared<HttpMessageStringViewBody>(body, len);
        if (passThrough_)
        {
            addHeader("content-length", std::to_string(bodyPtr_->length()));
        }
    }
    void setContentTypeCodeAndCustomString(ContentType type,
                                           const char *typeString,
                                           size_t typeStringLength) override
    {
        contentType_ = type;
        flagForParsingContentType_ = true;

        string_view sv(typeString, typeStringLength);
        bool haveHeader = sv.find("content-type: ") == 0;
        bool haveCRLF = sv.rfind("\r\n") == sv.size() - 2;

        size_t endOffset = 0;
        if (haveHeader)
            endOffset += 14;
        if (haveCRLF)
            endOffset += 2;
        setContentType(string_view{typeString + (haveHeader ? 14 : 0),
                                   typeStringLength - endOffset});
    }

    void setContentTypeString(const char *typeString,
                              size_t typeStringLength) override;

    void setCustomStatusCode(int code,
                             const char *message,
                             size_t messageLength) override
    {
        assert(code >= 0);
        customStatusCode_ = code;
        statusMessage_ = string_view{message, messageLength};
    }

    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, Cookie> cookies_;

    int customStatusCode_{-1};
    HttpStatusCode statusCode_{kUnknown};
    string_view statusMessage_;

    trantor::Date creationDate_;
    Version version_{Version::kHttp11};
    bool closeConnection_{false};
    mutable std::shared_ptr<HttpMessageBody> bodyPtr_;
    ssize_t expriedTime_{-1};
    std::string sendfileName_;
    SendfileRange sendfileRange_{0, 0};

    mutable std::shared_ptr<Json::Value> jsonPtr_;

    std::shared_ptr<trantor::MsgBuffer> fullHeaderString_;
    mutable std::shared_ptr<trantor::MsgBuffer> httpString_;
    mutable size_t datePos_{static_cast<size_t>(-1)};
    mutable int64_t httpStringDate_{-1};
    mutable bool flagForParsingJson_{false};
    mutable bool flagForSerializingJson_{true};
    mutable ContentType contentType_{CT_TEXT_PLAIN};
    mutable bool flagForParsingContentType_{false};
    mutable std::shared_ptr<std::string> jsonParsingErrorPtr_;
    mutable std::string contentTypeString_{"text/html; charset=utf-8"};
    bool passThrough_{false};
    void setContentType(const string_view &contentType)
    {
        contentTypeString_ =
            std::string(contentType.data(), contentType.size());
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
