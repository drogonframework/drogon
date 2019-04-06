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
    HttpResponse()
    {
    }
    /// Get the status code such as 200, 404
    virtual HttpStatusCode statusCode() = 0;

    /// Set the status code of the response.
    virtual void setStatusCode(HttpStatusCode code) = 0;

    // /// Set the status code and the status message of the response. Usually not used.
    // virtual void setStatusCode(HttpStatusCode code, const std::string &status_message) = 0;

    /// Get the creation timestamp of the response.
    virtual const trantor::Date &creationDate() const = 0;

    /// Set the http version, http1.0 or http1.1
    virtual void setVersion(const Version v) = 0;

    /// If @param on is false, the connection keeps alive on the condition that the client request has a
    //  'keep-alive' head, otherwise it is closed immediately after sending the last byte of the response.
    //  It's false by default when the response is created.
    virtual void setCloseConnection(bool on) = 0;

    /// Get the status set by the setCloseConnetion() method.
    virtual bool closeConnection() const = 0;

    /// Set the reponse content type, such as text/html, text/plaint, image/png and so on. If the content type
    /// is a text type, the character set is utf8.
    virtual void setContentTypeCode(ContentType type) = 0;

    /// Set the reponse content type and the character set.
    /// virtual void setContentTypeCodeAndCharacterSet(ContentType type, const std::string &charSet = "utf-8") = 0;

    /// Get the response content type.
    virtual ContentType getContentTypeCode() = 0;

    /// Get the header string identified by the @param key.
    /// If there is no the header, the @param defaultVal is retured.
    /// The @param key is case insensitive
    virtual const std::string &getHeader(const std::string &key, const std::string &defaultVal = std::string()) const = 0;
    virtual const std::string &getHeader(std::string &&key, const std::string &defaultVal = std::string()) const = 0;

    /// Get all headers of the response
    virtual const std::unordered_map<std::string, std::string> &headers() const = 0;

    /// Add a header.
    virtual void addHeader(const std::string &key, const std::string &value) = 0;
    virtual void addHeader(const std::string &key, std::string &&value) = 0;

    /// Add a cookie
    virtual void addCookie(const std::string &key, const std::string &value) = 0;
    virtual void addCookie(const Cookie &cookie) = 0;

    /// Get the cookie identified by the @param key. If there is no the cookie,
    /// If there is no the cookie, the @param defaultCookie is retured.
    virtual const Cookie &getCookie(const std::string &key, const Cookie &defaultCookie = Cookie()) const = 0;

    /// Get all cookies.
    virtual const std::unordered_map<std::string, Cookie> &cookies() const = 0;

    /// Remove the cookie identified by the @param key.
    virtual void removeCookie(const std::string &key) = 0;

    /// Set the response body(content). The @param body must match the content type
    virtual void setBody(const std::string &body) = 0;
    virtual void setBody(std::string &&body) = 0;

    /// Get the response body.
    virtual const std::string &getBody() const = 0;
    virtual std::string &getBody() = 0;

    /// Reset the reponse object to its initial state
    virtual void clear() = 0;

    /// Set the expiration time of the response cache in memory.
    /// in seconds, 0 means always cache, negative means not cache, default is -1.
    virtual void setExpiredTime(ssize_t expiredTime) = 0;

    /// Get the expiration time of the response.
    virtual ssize_t expiredTime() const = 0;

    /// Get the json object from the server response.
    /// If the response is not in json format, then a empty shared_ptr is be retured.
    virtual const std::shared_ptr<Json::Value> getJsonObject() const = 0;

    /// The following methods are a series of factory methods that help users create response objects.

    /// Create a normal response with a status code of 200ok and a content type of text/html.
    static HttpResponsePtr newHttpResponse();
    /// Create a response which returns a 404 page.
    static HttpResponsePtr newNotFoundResponse();
    /// Create a response which returns a json object. Its content type is set to set/json.
    static HttpResponsePtr newHttpJsonResponse(const Json::Value &data);
    /// Create a response that returns a page rendered by a view named @param viewName.
    /// @param data is the data displayed on the page.
    /// For more details, see the wiki pages, the "View" section.
    static HttpResponsePtr newHttpViewResponse(const std::string &viewName, const HttpViewData &data = HttpViewData());
    /// Create a response that returns a 302 Found page, redirecting to another page located in the @param path.
    static HttpResponsePtr newLocationRedirectResponse(const std::string &path);
    /// Create a response that returns a file to the client.
    /**
     * @param fullPath is the full path to the file.
     * If @param attachmentFileName is not empty, the browser does not open the file, but saves it as an attachment.
     * If the @param type is CT_NONE, the content type is set by drogon based on the file extension.
     */
    static HttpResponsePtr newFileResponse(const std::string &fullPath, const std::string &attachmentFileName = "", ContentType type = CT_NONE);

    virtual ~HttpResponse() {}
};

} // namespace drogon
