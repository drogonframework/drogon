/**
 *
 *  HttpRequest.h
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

#include <drogon/HttpTypes.h>
#include <drogon/Session.h>
#include <drogon/UploadFile.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <unordered_map>

namespace drogon
{
class HttpRequest;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;

/// Abstract class for webapp developer to get or set the Http request;
class HttpRequest
{
  public:
    enum Version
    {
        kUnknown = 0,
        kHttp10 = 1,
        kHttp11 = 2
    };
    /// Return the method string of the request, such as GET, POST, etc.
    virtual const char *methodString() const = 0;
    const char *getMethodString() const
    {
        return methodString();
    }

    /// Return the enum type method of the request.
    virtual HttpMethod method() const = 0;
    HttpMethod getMethod() const
    {
        return method();
    }

    /// Get the header string identified by the @param field
    virtual const std::string &getHeader(
        const std::string &field,
        const std::string &defaultVal = std::string()) const = 0;
    virtual const std::string &getHeader(
        std::string &&field,
        const std::string &defaultVal = std::string()) const = 0;

    /// Set the header string identified by the @param field
    virtual void addHeader(const std::string &field,
                           const std::string &value) = 0;

    /// Get the cookie string identified by the @param field
    virtual const std::string &getCookie(
        const std::string &field,
        const std::string &defaultVal = std::string()) const = 0;

    /// Get all headers of the request
    virtual const std::unordered_map<std::string, std::string> &headers()
        const = 0;
    const std::unordered_map<std::string, std::string> &getHeaders() const
    {
        return headers();
    }

    /// Get all cookies of the request
    virtual const std::unordered_map<std::string, std::string> &cookies()
        const = 0;
    const std::unordered_map<std::string, std::string> &getCookies() const
    {
        return cookies();
    }

    /// Get the query string of the request.
    /**
     * If the http method is GET, the query string is the substring after the
     * '?' in the URL string. If the http method is POST, the query string is
     * the content(body) string of the HTTP request.
     */
    virtual const std::string &query() const = 0;
    const std::string &getQuery() const
    {
        return query();
    }

    /// Get the content string of the request, which is the body part of the
    /// request.
    virtual const std::string &body() const = 0;
    const std::string &getBody() const
    {
        return body();
    }

    /// Get the path of the request.
    virtual const std::string &path() const = 0;
    const std::string &getPath() const
    {
        return path();
    }

    /// Get the matched path pattern after routing
    virtual const string_view &matchedPathPattern() const = 0;
    const string_view &getMatchedPathPattern() const
    {
        return matchedPathPattern();
    }

    /// Return the enum type version of the request.
    /**
     * kHttp10 means Http version is 1.0
     * kHttp11 means Http verison is 1.1
     */
    virtual Version version() const = 0;
    Version getVersion() const
    {
        return version();
    }

    /// Get the session to which the request belongs.
    virtual SessionPtr session() const = 0;
    SessionPtr getSession() const
    {
        return session();
    }

    /// Get parameters of the request.
    virtual const std::unordered_map<std::string, std::string> &parameters()
        const = 0;
    const std::unordered_map<std::string, std::string> &getParameters() const
    {
        return parameters();
    }

    /// Get a parameter identified by the @param key
    virtual const std::string &getParameter(
        const std::string &key,
        const std::string &defaultVal = std::string()) const = 0;

    /// Return the remote IP address and port
    virtual const trantor::InetAddress &peerAddr() const = 0;
    const trantor::InetAddress &getPeerAddr() const
    {
        return peerAddr();
    }

    /// Return the local IP address and port
    virtual const trantor::InetAddress &localAddr() const = 0;
    const trantor::InetAddress &getLocalAddr() const
    {
        return localAddr();
    }

    /// Return the creation timestamp set by the framework.
    virtual const trantor::Date &creationDate() const = 0;
    const trantor::Date &getCreationDate() const
    {
        return creationDate();
    }

    /// Get the Json object of the request
    /**
     * The content type of the request must be 'application/json', and the query
     * string (the part after the question mark in the URI) must be empty,
     * otherwise the method returns an empty shared_ptr object.
     */
    virtual const std::shared_ptr<Json::Value> jsonObject() const = 0;
    const std::shared_ptr<Json::Value> getJsonObject() const
    {
        return jsonObject();
    }

    /// Get the content type
    virtual ContentType contentType() const = 0;
    ContentType getContentType() const
    {
        return contentType();
    }

    /// Set the Http method
    virtual void setMethod(const HttpMethod method) = 0;

    /// Set the path of the request
    virtual void setPath(const std::string &path) = 0;

    /// Set the parameter of the request
    virtual void setParameter(const std::string &key,
                              const std::string &value) = 0;

    /// Set or get the content type
    virtual void setContentTypeCode(const ContentType type) = 0;

    /// Add a cookie
    virtual void addCookie(const std::string &key,
                           const std::string &value) = 0;

    /// The following methods are a series of factory methods that help users
    /// create request objects.

    /// Create a normal request with http method Get and version Http1.1.
    static HttpRequestPtr newHttpRequest();

    /// Create a http request with:
    /// Method: Get
    /// Version: Http1.1
    /// Content type: application/json, the @param data is serialized into the
    /// content of the request.
    static HttpRequestPtr newHttpJsonRequest(const Json::Value &data);

    /// Create a http request with:
    /// Method: Post
    /// Version: Http1.1
    /// Content type: application/x-www-form-urlencoded
    static HttpRequestPtr newHttpFormPostRequest();

    /// Create a http file upload request with:
    /// Method: Post
    /// Version: Http1.1
    /// Content type: multipart/form-data
    /// The @param files represents pload files which are transferred to the
    /// server via the multipart/form-data format
    static HttpRequestPtr newFileUploadRequest(
        const std::vector<UploadFile> &files);

    virtual ~HttpRequest()
    {
    }
};

}  // namespace drogon
