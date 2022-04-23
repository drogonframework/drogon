/**
 *
 *  @file HttpRequest.h
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

#include <drogon/exports.h>
#include <drogon/utils/string_view.h>
#include <drogon/utils/optional.h>
#include <drogon/utils/Utilities.h>
#include <drogon/DrClassMap.h>
#include <drogon/HttpTypes.h>
#include <drogon/Session.h>
#include <drogon/Attribute.h>
#include <drogon/UploadFile.h>
#include <json/json.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace drogon
{
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

/**
 * @brief This template is used to convert a request object to a custom
 * type object. Users must specialize the template for a particular type.
 */
template <typename T>
T fromRequest(const HttpRequest &)
{
    LOG_ERROR << "You must specialize the fromRequest template for the type of "
              << DrClassMap::demangle(typeid(T).name());
    exit(1);
}

/**
 * @brief This template is used to create a request object from a custom
 * type object by calling the newCustomHttpRequest(). Users must specialize
 * the template for a particular type.
 */
template <typename T>
HttpRequestPtr toRequest(T &&)
{
    LOG_ERROR << "You must specialize the toRequest template for the type of "
              << DrClassMap::demangle(typeid(T).name());
    exit(1);
}

template <>
HttpRequestPtr toRequest<const Json::Value &>(const Json::Value &pJson);
template <>
HttpRequestPtr toRequest(Json::Value &&pJson);
template <>
inline HttpRequestPtr toRequest<Json::Value &>(Json::Value &pJson)
{
    return toRequest((const Json::Value &)pJson);
}

template <>
std::shared_ptr<Json::Value> fromRequest(const HttpRequest &req);

/// Abstract class for webapp developer to get or set the Http request;
class DROGON_EXPORT HttpRequest
{
  public:
    /**
     * @brief This template enables implicit type conversion. For using this
     * template, user must specialize the fromRequest template. For example a
     * shared_ptr<Json::Value> specialization version is available above, so
     * we can use the following code to get a json object:
     * @code
       std::shared_ptr<Json::Value> jsonPtr = *requestPtr;
       @endcode
     * With this template, user can use their favorite JSON library instead of
     * the default jsoncpp library or convert the request to an object of any
     * custom type.
     */
    template <typename T>
    operator T() const
    {
        return fromRequest<T>(*this);
    }

    /**
     * @brief This template enables explicit type conversion, see the above
     * template.
     */
    template <typename T>
    T as() const
    {
        return fromRequest<T>(*this);
    }

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

    /// Get the header string identified by the key parameter.
    /**
     * @note
     * If there is no the header, a empty string is retured.
     * The key is case insensitive
     */
    virtual const std::string &getHeader(std::string key) const = 0;

    /**
     * @brief Set the header string identified by the field parameter
     *
     * @param field The field parameter is transformed to lower case before
     * storing.
     * @param value The value of the header.
     */
    virtual void addHeader(std::string field, const std::string &value) = 0;
    virtual void addHeader(std::string field, std::string &&value) = 0;

    /**
     * @brief  Remove the header identified by the key parameter.
     *
     * @param key The key is case insensitive
     */
    virtual void removeHeader(std::string key) = 0;

    /// Get the cookie string identified by the field parameter
    virtual const std::string &getCookie(const std::string &field) const = 0;

    /// Get all headers of the request
    virtual const std::unordered_map<std::string, std::string> &headers()
        const = 0;

    /// Get all headers of the request
    const std::unordered_map<std::string, std::string> &getHeaders() const
    {
        return headers();
    }

    /// Get all cookies of the request
    virtual const std::unordered_map<std::string, std::string> &cookies()
        const = 0;

    /// Get all cookies of the request
    const std::unordered_map<std::string, std::string> &getCookies() const
    {
        return cookies();
    }

    /// Get the query string of the request.
    /**
     * The query string is the substring after the '?' in the URL string.
     */
    virtual const std::string &query() const = 0;

    /// Get the query string of the request.
    const std::string &getQuery() const
    {
        return query();
    }

    /// Get the content string of the request, which is the body part of the
    /// request.
    string_view body() const
    {
        return string_view(bodyData(), bodyLength());
    }

    /// Get the content string of the request, which is the body part of the
    /// request.
    string_view getBody() const
    {
        return body();
    }
    virtual const char *bodyData() const = 0;
    virtual size_t bodyLength() const = 0;

    /// Set the content string of the request.
    virtual void setBody(const std::string &body) = 0;

    /// Set the content string of the request.
    virtual void setBody(std::string &&body) = 0;

    /// Get the path of the request.
    virtual const std::string &path() const = 0;

    /// Get the path of the request.
    const std::string &getPath() const
    {
        return path();
    }

    /// Get the matched path pattern after routing
    string_view getMatchedPathPattern() const
    {
        return matchedPathPattern();
    }

    /// Get the matched path pattern after routing
    string_view matchedPathPattern() const
    {
        return string_view(matchedPathPatternData(),
                           matchedPathPatternLength());
    }
    virtual const char *matchedPathPatternData() const = 0;
    virtual size_t matchedPathPatternLength() const = 0;

    /// Return the string of http version of request, such as HTTP/1.0,
    /// HTTP/1.1, etc.
    virtual const char *versionString() const = 0;
    const char *getVersionString() const
    {
        return versionString();
    }

    /// Return the enum type version of the request.
    /**
     * kHttp10 means Http version is 1.0
     * kHttp11 means Http verison is 1.1
     */
    virtual Version version() const = 0;

    /// Return the enum type version of the request.
    Version getVersion() const
    {
        return version();
    }

    /// Get the session to which the request belongs.
    virtual const SessionPtr &session() const = 0;

    /// Get the session to which the request belongs.
    const SessionPtr &getSession() const
    {
        return session();
    }

    /// Get the attributes store, users can add/get any type of data to/from
    /// this store
    virtual const AttributesPtr &attributes() const = 0;

    /// Get the attributes store, users can add/get any type of data to/from
    /// this store
    const AttributesPtr &getAttributes() const
    {
        return attributes();
    }

    /// Get parameters of the request.
    virtual const std::unordered_map<std::string, std::string> &parameters()
        const = 0;

    /// Get parameters of the request.
    const std::unordered_map<std::string, std::string> &getParameters() const
    {
        return parameters();
    }

    /// Get a parameter identified by the @param key
    virtual const std::string &getParameter(const std::string &key) const = 0;

    /**
     * @brief Get the optional parameter identified by the @param key. if the
     * parameter doesn't exist, or the original parameter can't be converted to
     * a T type object, an empty optional object is returned.
     *
     * @tparam T
     * @param key
     * @return optional<T>
     */
    template <typename T>
    optional<T> getOptionalParameter(const std::string &key)
    {
        auto &params = getParameters();
        auto it = params.find(key);
        if (it != params.end())
        {
            try
            {
                return optional<T>(drogon::utils::fromString<T>(it->second));
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << e.what();
                return optional<T>{};
            }
        }
        else
        {
            return optional<T>{};
        }
    }

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
     * The content type of the request must be 'application/json',
     * otherwise the method returns an empty shared_ptr object.
     */
    virtual const std::shared_ptr<Json::Value> &jsonObject() const = 0;

    /// Get the Json object of the request
    const std::shared_ptr<Json::Value> &getJsonObject() const
    {
        return jsonObject();
    }

    /**
     * @brief Get the error message of parsing the JSON body received from peer.
     * This method usually is called after getting a empty shared_ptr object
     * by the getJsonObject() method.
     *
     * @return const std::string& The error message. An empty string is returned
     * when no error occurs.
     */
    virtual const std::string &getJsonError() const = 0;

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

    /**
     * @brief The default behavior is to encode the value of setPath
     * using urlEncode. Setting the path encode to false avoid the
     * value of path will be changed by the library
     *
     * @param bool true --> the path will be url encoded
     *             false --> using value of path as it is set
     */
    virtual void setPathEncode(bool) = 0;

    /// Set the parameter of the request
    virtual void setParameter(const std::string &key,
                              const std::string &value) = 0;

    /// Set or get the content type
    virtual void setContentTypeCode(const ContentType type) = 0;

    /// Set the content-type string, The string may contain the header name and
    /// CRLF. Or just the MIME type
    //
    /// For example, "content-type: text/plain\r\n" or "text/plain"
    void setContentTypeString(const string_view &typeString)
    {
        setContentTypeString(typeString.data(), typeString.size());
    }

    /// Set the request content-type string, The string
    /// must contain the header name and CRLF.
    /// For example, "content-type: text/plain\r\n"
    virtual void setCustomContentTypeString(const std::string &type) = 0;

    /// Add a cookie
    virtual void addCookie(const std::string &key,
                           const std::string &value) = 0;

    /**
     * @brief Set the request object to the pass-through mode or not. It's not
     * by default when a new request object is created.
     * In pass-through mode, no addtional headers (including user-agent,
     * connection, etc.) are added to the request. This mode is useful for some
     * applications such as a proxy.
     *
     * @param flag
     */
    virtual void setPassThrough(bool flag) = 0;

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

    /**
     * @brief Create a custom HTTP request object. For using this template,
     * users must specialize the toRequest template.
     */
    template <typename T>
    static HttpRequestPtr newCustomHttpRequest(T &&obj)
    {
        return toRequest(std::forward<T>(obj));
    }

    virtual bool isOnSecureConnection() const noexcept = 0;
    virtual void setContentTypeString(const char *typeString,
                                      size_t typeStringLength) = 0;

    virtual ~HttpRequest()
    {
    }
};

template <>
inline HttpRequestPtr toRequest<const Json::Value &>(const Json::Value &pJson)
{
    return HttpRequest::newHttpJsonRequest(pJson);
}

template <>
inline HttpRequestPtr toRequest(Json::Value &&pJson)
{
    return HttpRequest::newHttpJsonRequest(std::move(pJson));
}

template <>
inline std::shared_ptr<Json::Value> fromRequest(const HttpRequest &req)
{
    return req.getJsonObject();
}

}  // namespace drogon
