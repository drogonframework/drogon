/**
 *
 *  @file HttpClient.h
 *
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by the MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <drogon/exports.h>
#include <drogon/HttpTypes.h>
#include <drogon/drogon_callbacks.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <functional>
#include <memory>
#include <future>
#include "drogon/HttpBinder.h"

#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
class HttpClient;
using HttpClientPtr = std::shared_ptr<HttpClient>;
#ifdef __cpp_impl_coroutine
namespace internal
{
struct HttpRespAwaiter : public CallbackAwaiter<HttpResponsePtr>
{
    HttpRespAwaiter(HttpClient *client, HttpRequestPtr req, double timeout)
        : client_(client), req_(std::move(req)), timeout_(timeout)
    {
    }

    void await_suspend(std::coroutine_handle<> handle);

  private:
    HttpClient *client_;
    HttpRequestPtr req_;
    double timeout_;
};

}  // namespace internal
#endif

/// Asynchronous http client
/**
 * HttpClient implementation object uses the HttpAppFramework's event loop by
 * default, so you should call app().run() to make the client work.
 * Each HttpClient object establishes a persistent connection with the server.
 * If the connection is broken, the client attempts to reconnect
 * when calling the sendRequest method.
 *
 * Using the static mathod newHttpClient(...) to get shared_ptr of the object
 * implementing the class, the shared_ptr is retained in the framework until all
 * response callbacks are invoked without fear of accidental deconstruction.
 *
 */
class DROGON_EXPORT HttpClient : public trantor::NonCopyable
{
  public:
    /**
     * @brief Send a request asynchronously to the server
     *
     * @param req The request sent to the server.
     * @param callback The callback is called when the response is received from
     * the server.
     * @param timeout In seconds. If the response is not received within the
     * timeout, the callback is called with `ReqResult::Timeout` and an empty
     * response. The zero value by default disables the timeout.
     *
     * @note
     * The request object is altered(some headers are added to it) before it is
     * sent, so calling this method with a same request object in different
     * thread is dangerous.
     * Please be careful when using timeout on an non-idempotent request.
     */
    virtual void sendRequest(const HttpRequestPtr &req,
                             const HttpReqCallback &callback,
                             double timeout = 0) = 0;

    /**
     * @brief Send a request asynchronously to the server
     *
     * @param req The request sent to the server.
     * @param callback The callback is called when the response is received from
     * the server.
     * @param timeout In seconds. If the response is not received within
     * the timeout, the callback is called with `ReqResult::Timeout` and an
     * empty response. The zero value by default disables the timeout.
     *
     * @note
     * The request object is altered(some headers are added to it) before it is
     * sent, so calling this method with a same request object in different
     * thread is dangerous.
     * Please be careful when using timeout on an non-idempotent request.
     */
    virtual void sendRequest(const HttpRequestPtr &req,
                             HttpReqCallback &&callback,
                             double timeout = 0) = 0;

    /**
     * @brief Send a request synchronously to the server and return the
     * response.
     *
     * @param req
     * @param timeout In seconds. If the response is not received within the
     * timeout, the `ReqResult::Timeout` and an empty response is returned. The
     * zero value by default disables the timeout.
     *
     * @return std::pair<ReqResult, HttpResponsePtr>
     * @note Never call this function in the event loop thread of the
     * client (partially in the callback function of the asynchronous
     * sendRequest method), otherwise the thread will be blocked forever.
     * Please be careful when using timeout on an non-idempotent request.
     */
    std::pair<ReqResult, HttpResponsePtr> sendRequest(const HttpRequestPtr &req,
                                                      double timeout = 0)
    {
        assert(!getLoop()->isInLoopThread() &&
               "Deadlock detected! Calling a sync API from the same loop as "
               "the HTTP client processes on will deadlock the event loop");
        std::promise<std::pair<ReqResult, HttpResponsePtr>> prom;
        auto f = prom.get_future();
        sendRequest(
            req,
            [&prom](ReqResult r, const HttpResponsePtr &resp) {
                prom.set_value({r, resp});
            },
            timeout);
        return f.get();
    }

#ifdef __cpp_impl_coroutine
    /**
     * @brief Send a request via coroutines to the server and return an
     * awaiter what could be `co_await`-ed to retrieve the response
     * (HttpResponsePtr)
     *
     * @param req
     * @param timeout In seconds. If the response is not received within the
     * timeout, A `std::runtime_error` with the message "Timeout" is thrown.
     * The zero value by default disables the timeout.
     *
     * @return internal::HttpRespAwaiter. Await on it to get the response
     */
    internal::HttpRespAwaiter sendRequestCoro(HttpRequestPtr req,
                                              double timeout = 0)
    {
        return internal::HttpRespAwaiter(this, std::move(req), timeout);
    }
#endif

    /// Set the pipelining depth, which is the number of requests that are not
    /// responding.
    /**
     * If this method is not called, the default depth value is 0 which means
     * the pipelining is disabled. For details about pipelining, see
     * rfc2616-8.1.2.2
     */
    virtual void setPipeliningDepth(size_t depth) = 0;

    /// Enable cookies for the client
    /**
     * @param flag if the parameter is true, all requests sent by the client
     * carry the cookies set by the server side. Cookies are disabled by
     * default.
     */
    virtual void enableCookies(bool flag = true) = 0;

    /// Add a cookie to the client
    /**
     * @note
     * These methods are independent of the enableCookies() method. Whether the
     * enableCookies() is called with true or false, the cookies added by these
     * methods will be sent to the server.
     */
    virtual void addCookie(const std::string &key,
                           const std::string &value) = 0;

    /// Add a cookie to the client
    /**
     * @note
     * These methods are independent of the enableCookies() method. Whether the
     * enableCookies() is called with true or false, the cookies added by these
     * methods will be sent to the server.
     */
    virtual void addCookie(const Cookie &cookie) = 0;

    /**
     * @brief Set the user_agent header, the default value is 'DrogonClient' if
     * this method is not used.
     *
     * @param userAgent The user_agent value, if it is empty, the user_agent
     * header is not sent to the server.
     */
    virtual void setUserAgent(const std::string &userAgent) = 0;

    /**
     * @brief Creaet a new HTTP client which use ip and port to connect to
     * server
     *
     * @param ip The ip address of the HTTP server
     * @param port The port of the HTTP server
     * @param useSSL if the parameter is set to true, the client connects to the
     * server using HTTPS.
     * @param loop If the loop parameter is set to nullptr, the client uses the
     * HttpAppFramework's event loop, otherwise it runs in the loop identified
     * by the parameter.
     * @param useOldTLS If the parameter is set to true, the TLS1.0/1.1 are
     * eanbled for HTTPS.
     * @param validateCert If the parameter is set to true, the client validates
     * the server certificate when SSL handshaking.
     * @return HttpClientPtr The smart pointer to the new client object.
     * @note: The ip parameter support for both ipv4 and ipv6 address
     */
    static HttpClientPtr newHttpClient(const std::string &ip,
                                       uint16_t port,
                                       bool useSSL = false,
                                       trantor::EventLoop *loop = nullptr,
                                       bool useOldTLS = false,
                                       bool validateCert = true);

    /// Get the event loop of the client;
    virtual trantor::EventLoop *getLoop() = 0;

    /// Get the number of bytes sent or received
    virtual size_t bytesSent() const = 0;
    virtual size_t bytesReceived() const = 0;

    virtual std::string host() const = 0;
    std::string getHost() const
    {
        return host();
    }

    virtual uint16_t port() const = 0;
    uint16_t getPort() const
    {
        return port();
    }

    virtual bool secure() const = 0;

    bool onDefaultPort() const
    {
        if (secure())
            return port() == 443;
        return port() == 80;
    }

    /**
     * @brief Set the client certificate used by the HTTP connection
     *
     * @param cert Path to the certificate
     * @param key Path to the certificate's private key
     * @note this method has no effect if the HTTP client is communicating via
     * unencrypted HTTP
     */
    virtual void setCertPath(const std::string &cert,
                             const std::string &key) = 0;

    /**
     * @brief Supplies command style options for `SSL_CONF_cmd`
     *
     * @param sslConfCmds options for SSL_CONF_cmd
     * @note this method has no effect if the HTTP client is communicating via
     * unencrypted HTTP
     * @code
     * addSSLConfigs({{"-dhparam", "/path/to/dhparam"}, {"-strict", ""}});
     * @endcode
     */
    virtual void addSSLConfigs(
        const std::vector<std::pair<std::string, std::string>>
            &sslConfCmds) = 0;

    /// Create a Http client using the hostString to connect to server
    /**
     *
     * @param hostString this parameter must be prefixed by 'http://' or
     * 'https://'.
     *
     * Examples for hostString:
     * @code
       https://www.baidu.com
       http://www.baidu.com
       https://127.0.0.1:8080/
       http://127.0.0.1
       http://[::1]:8080/   //IPv6 address must be enclosed in [], rfc2732
       @endcode
     *
     * @param loop If the loop parameter is set to nullptr, the client uses the
     * HttpAppFramework's event loop, otherwise it runs in the loop identified
     * by the parameter.
     *
     * @param useOldTLS If the parameter is set to true, the TLS1.0/1.1 are
     * enabled for HTTPS.
     * @note
     *
     * @param validateCert If the parameter is set to true, the client validates
     * the server certificate when SSL handshaking.
     *
     * @note Don't add path and parameters in hostString, the request path and
     * parameters should be set in HttpRequestPtr when calling the sendRequest()
     * method.
     *
     */
    static HttpClientPtr newHttpClient(const std::string &hostString,
                                       trantor::EventLoop *loop = nullptr,
                                       bool useOldTLS = false,
                                       bool validateCert = true);

    virtual ~HttpClient()
    {
    }

  protected:
    HttpClient() = default;
};

#ifdef __cpp_impl_coroutine
inline void internal::HttpRespAwaiter::await_suspend(
    std::coroutine_handle<> handle)
{
    assert(client_ != nullptr);
    assert(req_ != nullptr);
    client_->sendRequest(
        req_,
        [handle = std::move(handle), this](ReqResult result,
                                           const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
                setValue(resp);
            else
            {
                std::stringstream ss;
                ss << result;
                setException(
                    std::make_exception_ptr(std::runtime_error(ss.str())));
            }
            handle.resume();
        },
        timeout_);
}
#endif

}  // namespace drogon
