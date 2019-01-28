/**
 *  HttpResponse.h
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

#include <drogon/HttpViewData.h>
#include <drogon/Cookie.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include <string>
#include <memory>

namespace drogon
{
/// Abstract class for webapp developer to get or set the Http response;
class HttpResponse;
typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
class HttpResponse
{
  public:
    explicit HttpResponse()
    {
    }

    virtual HttpStatusCode statusCode() = 0;

    virtual const trantor::Date &createDate() const = 0;

    virtual void setStatusCode(HttpStatusCode code) = 0;

    virtual void setStatusCode(HttpStatusCode code, const std::string &status_message) = 0;

    virtual void setVersion(const Version v) = 0;

    virtual void setCloseConnection(bool on) = 0;

    virtual bool closeConnection() const = 0;

    virtual void setContentTypeCode(ContentType type) = 0;

    virtual void setContentTypeCodeAndCharacterSet(ContentType type, const std::string &charSet = "utf-8") = 0;

    virtual ContentType getContentTypeCode() = 0;

    virtual const std::string &getHeader(const std::string &key, const std::string &defaultVal = std::string()) const = 0;

    virtual const std::string &getHeader(std::string &&key, const std::string &defaultVal = std::string()) const = 0;

    virtual void addHeader(const std::string &key, const std::string &value) = 0;

    virtual void addHeader(const std::string &key, std::string &&value) = 0;

    virtual void addHeader(const char *start, const char *colon, const char *end) = 0;

    virtual void addCookie(const std::string &key, const std::string &value) = 0;

    virtual void addCookie(const Cookie &cookie) = 0;

    virtual const Cookie &getCookie(const std::string &key, const Cookie &defaultCookie = Cookie()) const = 0;

    virtual const std::unordered_map<std::string, Cookie> &cookies() const = 0;

    virtual void removeCookie(const std::string &key) = 0;

    virtual void setBody(const std::string &body) = 0;

    virtual void setBody(std::string &&body) = 0;

    virtual void redirect(const std::string &url) = 0;

    virtual void clear() = 0;

    virtual void setExpiredTime(ssize_t expiredTime) = 0;
    virtual ssize_t expiredTime() const = 0;

    virtual const std::string &getBody() const = 0;
    virtual std::string &getBody() = 0;

    virtual const std::shared_ptr<Json::Value> getJsonObject() const = 0;
    static HttpResponsePtr newHttpResponse();
    static HttpResponsePtr newNotFoundResponse();
    static HttpResponsePtr newHttpJsonResponse(const Json::Value &data);
    static HttpResponsePtr newHttpViewResponse(const std::string &viewName, const HttpViewData &data = HttpViewData());
    static HttpResponsePtr newLocationRedirectResponse(const std::string &path);
    static HttpResponsePtr newFileResponse(const std::string &fullPath, const std::string &attachmentFileName = "", bool asAttachment = true);

    virtual ~HttpResponse() {}
};

} // namespace drogon
