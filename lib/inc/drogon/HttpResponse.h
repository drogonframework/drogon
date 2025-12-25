/**
 *  @file HttpResponse.h
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
#include <trantor/net/Certificate.h>
#include <trantor/net/callbacks.h>
#include <trantor/net/AsyncStream.h>
#include <drogon/DrClassMap.h>
#include <drogon/Cookie.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <drogon/HttpViewData.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <string_view>

namespace drogon
{
/// Abstract class for webapp developer to get or set the Http response;
class HttpResponse;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

/**
 * @brief This template is used to convert a response object to a custom
 * type object. Users must specialize the template for a particular type.
 */
template <typename T>
T fromResponse(const HttpResponse &)
{
    LOG_ERROR
        << "You must specialize the fromResponse template for the type of "
        << DrClassMap::demangle(typeid(T).name());
    exit(1);
}

/**
 * @brief This template is used to create a response object from a custom
 * type object by calling the newCustomHttpResponse(). Users must specialize
 * the template for a particular type.
 */
template <typename T>
HttpResponsePtr toResponse(T &&)
{
    LOG_ERROR << "You must specialize the toResponse template for the type of "
              << DrClassMap::demangle(typeid(T).name());
    exit(1);
}

template <>
HttpResponsePtr toResponse<const Json::Value &>(const Json::Value &pJson);
template <>
HttpResponsePtr toResponse(Json::Value &&pJson);

template <>
inline HttpResponsePtr toResponse<Json::Value &>(Json::Value &pJson)
{
    return toResponse((const Json::Value &)pJson);
}

class DROGON_EXPORT ResponseStream
{
  public:
    explicit ResponseStream(trantor::AsyncStreamPtr asyncStream)
        : asyncStream_(std::move(asyncStream))
    {
    }

    ~ResponseStream()
    {
        close();
    }

    bool send(const std::string &data)
    {
        if (!asyncStream_)
        {
            return false;
        }
        std::ostringstream oss;
        oss << std::hex << data.length() << "\r\n";
        oss << data << "\r\n";
        return asyncStream_->send(oss.str());
    }

    void close()
    {
        if (asyncStream_)
        {
            static std::string closeStream{"0\r\n\r\n"};
            asyncStream_->send(closeStream);
            asyncStream_->close();
            asyncStream_.reset();
        }
    }

  private:
    trantor::AsyncStreamPtr asyncStream_;
};

using ResponseStreamPtr = std::unique_ptr<ResponseStream>;

class DROGON_EXPORT HttpResponse
{
  public:
    /**
     * @brief This template enables automatic type conversion. For using this
     * template, user must specialize the fromResponse template. For example a
     * shared_ptr<Json::Value> specialization version is available above, so
     * we can use the following code to get a json object:
     * @code
     *  std::shared_ptr<Json::Value> jsonPtr = *responsePtr;
     *  @endcode
     * With this template, user can use their favorite JSON library instead of
     * the default jsoncpp library or convert the response to an object of any
     * custom type.
     */
    template <typename T>
    operator T() const
    {
        return fromResponse<T>(*this);
    }

    /**
     * @brief This template enables explicit type conversion, see the above
     * template.
     */
    template <typename T>
    T as() const
    {
        return fromResponse<T>(*this);
    }

    /// Get the status code such as 200, 404
    virtual HttpStatusCode statusCode() const = 0;

    HttpStatusCode getStatusCode() const
    {
        return statusCode();
    }

    /// Set the status code of the response.
    virtual void setStatusCode(HttpStatusCode code) = 0;

    void setCustomStatusCode(int code,
                             std::string_view message = std::string_view{})
    {
        setCustomStatusCode(code, message.data(), message.length());
    }

    /// Get the creation timestamp of the response.
    virtual const trantor::Date &creationDate() const = 0;

    const trantor::Date &getCreationDate() const
    {
        return creationDate();
    }

    /// Set the http version, http1.0 or http1.1
    virtual void setVersion(const Version v) = 0;

    /// Set if close the connection after the request is sent.
    /**
     * @param on if the parameter is false, the connection keeps alive on the
     * condition that the client request has a 'keep-alive' head, otherwise it
     * is closed immediately after sending the last byte of the response. It's
     * false by default when the response is created.
     */
    virtual void setCloseConnection(bool on) = 0;

    /// Get the status set by the setCloseConnection() method.
    virtual bool ifCloseConnection() const = 0;

    /// Set the response content type, such as text/html, text/plain, image/png
    /// and so on. If the content type
    /// is a text type, the character set is utf8.
    virtual void setContentTypeCode(ContentType type) = 0;

    /// Set the content-type string, The string may contain the header name and
    /// CRLF. Or just the MIME type For example, "content-type: text/plain\r\n"
    /// or "text/plain"
    void setContentTypeString(const std::string_view &typeString)
    {
        setContentTypeString(typeString.data(), typeString.size());
    }

    /// Set the response content type and the content-type string, The string
    /// may contain the header name and CRLF. Or just the MIME type
    /// For example, "content-type: text/plain\r\n" or "text/plain"
    void setContentTypeCodeAndCustomString(ContentType type,
                                           const std::string_view &typeString)
    {
        setContentTypeCodeAndCustomString(type,
                                          typeString.data(),
                                          typeString.length());
    }

    template <int N>
    void setContentTypeCodeAndCustomString(ContentType type,
                                           const char (&typeString)[N])
    {
        assert(N > 0);
        setContentTypeCodeAndCustomString(type, typeString, N - 1);
    }

    /// Set the response content type and the character set.
    /// virtual void setContentTypeCodeAndCharacterSet(ContentType type, const
    /// std::string &charSet = "utf-8") = 0;

    /// Get the response content type.
    virtual ContentType contentType() const = 0;

    ContentType getContentType() const
    {
        return contentType();
    }

    /// Get the header string identified by the key parameter.
    /**
     * @note
     * If there is no the header, a empty string is returned.
     * The key is case insensitive
     */
    virtual const std::string &getHeader(std::string key) const = 0;

    /**
     * @brief  Remove the header identified by the key parameter.
     *
     * @param key The key is case insensitive
     */
    virtual void removeHeader(std::string key) = 0;

    /// Get all headers of the response
    virtual const SafeStringMap<std::string> &headers() const = 0;

    /// Get all headers of the response
    const SafeStringMap<std::string> &getHeaders() const
    {
        return headers();
    }

    /**
     * @brief Set the header string identified by the field parameter
     *
     * @param field The field parameter is transformed to lower case before
     * storing.
     * @param value The value of the header.
     */
    virtual void addHeader(std::string field, const std::string &value) = 0;
    virtual void addHeader(std::string field, std::string &&value) = 0;

    /// Add a cookie
    virtual void addCookie(const std::string &key,
                           const std::string &value) = 0;

    /// Add a cookie
    virtual void addCookie(const Cookie &cookie) = 0;
    virtual void addCookie(Cookie &&cookie) = 0;

    /// Get the cookie identified by the key parameter.
    /// If there is no the cookie, the empty cookie is returned.
    virtual const Cookie &getCookie(const std::string &key) const = 0;

    /// Get all cookies.
    virtual const SafeStringMap<Cookie> &cookies() const = 0;

    /// Get all cookies.
    const SafeStringMap<Cookie> &getCookies() const
    {
        return cookies();
    }

    /// Remove the cookie identified by the key parameter.
    virtual void removeCookie(const std::string &key) = 0;

    /// Set the response body(content).
    /**
     * @note The body must match the content type
     */
    virtual void setBody(const std::string &body) = 0;

    /// Set the response body(content).
    virtual void setBody(std::string &&body) = 0;

    /// Set the response body(content).
    template <int N>
    void setBody(const char (&body)[N])
    {
        assert(strnlen(body, N) == N - 1);
        setBody(body, N - 1);
    }

    /// Get the response body.
    std::string_view body() const
    {
        return std::string_view{getBodyData(), getBodyLength()};
    }

    /// Get the response body.
    std::string_view getBody() const
    {
        return body();
    }

    /// Return the string of http version of request, such as HTTP/1.0,
    /// HTTP/1.1, etc.
    virtual const char *versionString() const = 0;

    const char *getVersionString() const
    {
        return versionString();
    }

    /// Return the enum type version of the response.
    /**
     * kHttp10 means Http version is 1.0
     * kHttp11 means Http version is 1.1
     */
    virtual Version version() const = 0;

    /// Return the enum type version of the response.
    Version getVersion() const
    {
        return version();
    }

    /// Reset the response object to its initial state
    virtual void clear() = 0;

    /// Set the expiration time of the response cache in memory.
    /// in seconds, 0 means always cache, negative means not cache, default is
    /// -1.
    virtual void setExpiredTime(ssize_t expiredTime) = 0;

    /// Get the expiration time of the response.
    virtual ssize_t expiredTime() const = 0;

    ssize_t getExpiredTime() const
    {
        return expiredTime();
    }

    /// Get the json object from the server response.
    /// If the response is not in json format, then a empty shared_ptr is
    /// returned.
    virtual const std::shared_ptr<Json::Value> &jsonObject() const = 0;

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

    /**
     * @brief Set the response object to the pass-through mode or not. It's not
     * by default when a new response object is created.
     * In pass-through mode, no additional headers (including server, date,
     * content-type and content-length, etc.) are added to the response. This
     * mode is useful for some applications such as a proxy.
     *
     * @param flag
     */
    virtual void setPassThrough(bool flag) = 0;

    /**
     * @brief Get the certificate of the peer, if any.
     * @return The certificate of the peer. nullptr is none.
     */
    virtual const trantor::CertificatePtr &peerCertificate() const = 0;

    const trantor::CertificatePtr &getPeerCertificate() const
    {
        return peerCertificate();
    }

    /* The following methods are a series of factory methods that help users
     * create response objects. */

    /// Create a normal response with a status code of 200ok and a content type
    /// of text/html.
    static HttpResponsePtr newHttpResponse();
    /// Create a response with a status code and a content type
    static HttpResponsePtr newHttpResponse(HttpStatusCode code,
                                           ContentType type);
    /// Create a response which returns a 404 page.
    static HttpResponsePtr newNotFoundResponse(
        const HttpRequestPtr &req = HttpRequestPtr());
    /// Create a response which returns a json object. Its content-type is set
    /// to application/json.
    static HttpResponsePtr newHttpJsonResponse(const Json::Value &data);
    /// Create a response which returns a json object. Its content-type is set
    /// to application/json.
    static HttpResponsePtr newHttpJsonResponse(Json::Value &&data);
    /// Create a response that returns a page rendered by a view named
    /// viewName.
    /**
     * @param viewName The name of the view
     * @param data is the data displayed on the page.
     * @note For more details, see the wiki pages, the "View" section.
     */
    static HttpResponsePtr newHttpViewResponse(
        const std::string &viewName,
        const HttpViewData &data = HttpViewData(),
        const HttpRequestPtr &req = HttpRequestPtr());

    /// Create a response that returns a redirection page, redirecting to
    /// another page located in the location parameter.
    /**
     * @param location The location to redirect
     * @param status The HTTP status code, k302Found by default. Users could set
     * it to one of the 301, 302, 303, 307, ...
     */
    static HttpResponsePtr newRedirectionResponse(
        const std::string &location,
        HttpStatusCode status = k302Found);

    /// Create a response that returns a file to the client.
    /**
     * @param fullPath is the full path to the file.
     * @param attachmentFileName if the parameter is not empty, the browser
     * does not open the file, but saves it as an attachment.
     * @param type the content type code. If the parameter is CT_NONE, the
     * content type is set by drogon based on the file extension and typeString.
     * Set it to CT_CUSTOM when no drogon internal content type matches.
     * @param typeString the MIME string of the content type.
     */
    static HttpResponsePtr newFileResponse(
        const std::string &fullPath,
        const std::string &attachmentFileName = "",
        ContentType type = CT_NONE,
        const std::string &typeString = "",
        const HttpRequestPtr &req = HttpRequestPtr());

    /// Create a response that returns part of a file to the client.
    /**
     * @brief If offset and length can not be satisfied, statusCode will be set
     * to k416RequestedRangeNotSatisfiable, and nothing else will be modified.
     *
     * @param fullPath is the full path to the file.
     * @param offset is the offset to begin sending, in bytes.
     * @param length is the total length to send, in bytes. In particular,
     * length = 0 means send all content from offset till end of file.
     * @param setContentRange whether set 'Content-Range' header automatically.
     * @param attachmentFileName if the parameter is not empty, the browser
     * does not open the file, but saves it as an attachment.
     * @param type the content type code. If the parameter is CT_NONE, the
     * content type is set by drogon based on the file extension and typeString.
     * Set it to CT_CUSTOM when no drogon internal content type matches.
     * @param typeString the MIME string of the content type.
     */
    static HttpResponsePtr newFileResponse(
        const std::string &fullPath,
        size_t offset,
        size_t length,
        bool setContentRange = true,
        const std::string &attachmentFileName = "",
        ContentType type = CT_NONE,
        const std::string &typeString = "",
        const HttpRequestPtr &req = HttpRequestPtr());

    /// Create a response that returns a file to the client from buffer in
    /// memory/stack
    /**
     * @param pBuffer is a uint 8 bit flat buffer for object/files in memory
     * @param bufferLength is the length of the expected buffer
     * @param attachmentFileName if the parameter is not empty, the browser
     * does not open the file, but saves it as an attachment.
     * @param type the content type code. If the parameter is CT_NONE, the
     * content type is set by drogon based on the file extension and typeString.
     * Set it to CT_CUSTOM when no drogon internal content type matches.
     * @param typeString the MIME string of the content type.
     */
    static HttpResponsePtr newFileResponse(
        const unsigned char *pBuffer,
        size_t bufferLength,
        const std::string &attachmentFileName = "",
        ContentType type = CT_NONE,
        const std::string &typeString = "");

    /// Create a response that returns a file to the client from a callback
    /// function
    /**
     * @note if the Connection is keep-alive and the Content-Length header is
     * not set, the stream data is sent with Transfer-Encoding: chunked.
     * @param callback function to retrieve the stream data (stream ends when a
     *                 zero size is returned) the callback will be called with
     *                 nullptr when the send is finished/interrupted so that it
     *                 cleans up its internals.
     * @param attachmentFileName if the parameter is not empty, the browser
     *                           does not open the file, but saves it as an
     *                           attachment.
     * @param type the content type code. If the parameter is CT_NONE, the
     *             content type is set by drogon based on the file extension and
     *             typeString. Set it to CT_CUSTOM when no drogon internal
     *             content type matches.
     * @param typeString the MIME string of the content type.
     */
    static HttpResponsePtr newStreamResponse(
        const std::function<std::size_t(char *, std::size_t)> &callback,
        const std::string &attachmentFileName = "",
        ContentType type = CT_NONE,
        const std::string &typeString = "",
        const HttpRequestPtr &req = HttpRequestPtr());

    /// Create a response that allows sending asynchronous data from a callback
    /// function
    /**
     * @note Async streams are always sent with Transfer-Encoding: chunked.
     * @param callback function that receives the asynchronous HTTP stream. You
     *                 may call the stream->send() method to transmit new data.
     *                 The send method will return true as long as the stream is
     *                 still open. Once you have finished sending data, or the
     *                 stream->send() function returned false, you should call
     *                 stream->close() to gracefully close the chunked transfer.
     * @param disableKickoffTimeout set this to true to disable trantors default
     *                              kickoff timeout. This is useful if you need
     *                              long running asynchronous streams.
     */
    static HttpResponsePtr newAsyncStreamResponse(
        const std::function<void(ResponseStreamPtr)> &callback,
        bool disableKickoffTimeout = false);

    /**
     * @brief Create a custom HTTP response object. For using this template,
     * users must specialize the toResponse template.
     */
    template <typename T>
    static HttpResponsePtr newCustomHttpResponse(T &&obj)
    {
        return toResponse(std::forward<T>(obj));
    }

    /**
     * @brief If the response is a file response (i.e. created by
     * newFileResponse) returns the path on the filesystem. Otherwise a
     * empty string.
     */
    virtual const std::string &sendfileName() const = 0;

    /**
     * @brief Returns the range of the file response as a pair ot size_t
     * (offset, length). Length of 0 means the entire file is sent. Behavior of
     * this function is undefined if the response if not a file response
     */
    using SendfileRange = std::pair<size_t, size_t>;  // { offset, length }
    virtual const SendfileRange &sendfileRange() const = 0;

    /**
     * @brief If the response is a stream response (i.e. created by
     * newStreamResponse) returns the callback function. Otherwise a
     * null function.
     */
    virtual const std::function<std::size_t(char *, std::size_t)> &
    streamCallback() const = 0;

    /**
     * @brief If the response is a async stream response (i.e. created by
     * asyncStreamCallback) returns the stream ptr.
     */
    virtual const std::function<void(ResponseStreamPtr)> &asyncStreamCallback()
        const = 0;

    /**
     * @brief Returns the content type associated with the response
     */
    virtual std::string contentTypeString() const = 0;

    virtual ~HttpResponse()
    {
    }

  private:
    virtual void setBody(const char *body, size_t len) = 0;
    virtual const char *getBodyData() const = 0;
    virtual size_t getBodyLength() const = 0;
    virtual void setContentTypeCodeAndCustomString(ContentType type,
                                                   const char *typeString,
                                                   size_t typeStringLength) = 0;
    virtual void setContentTypeString(const char *typeString,
                                      size_t typeStringLength) = 0;
    virtual void setCustomStatusCode(int code,
                                     const char *message,
                                     size_t messageLength) = 0;
};

template <>
inline HttpResponsePtr toResponse<const Json::Value &>(const Json::Value &pJson)
{
    return HttpResponse::newHttpJsonResponse(pJson);
}

template <>
inline HttpResponsePtr toResponse(Json::Value &&pJson)
{
    return HttpResponse::newHttpJsonResponse(std::move(pJson));
}

template <>
inline std::shared_ptr<Json::Value> fromResponse(const HttpResponse &resp)
{
    return resp.getJsonObject();
}
}  // namespace drogon
