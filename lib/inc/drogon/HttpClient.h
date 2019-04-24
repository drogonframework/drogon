/**
 *
 *  HttpClient.h
 * 
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

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <functional>
#include <memory>
namespace drogon
{

class HttpClient;
typedef std::shared_ptr<HttpClient> HttpClientPtr;

typedef std::function<void(ReqResult, const HttpResponsePtr &)> HttpReqCallback;

/// Asynchronous http client
/**
 * HttpClient implementation object uses the HttpAppFramework's event loop by default,
 * so you should call app().run() to make the client work.
 * Each HttpClient object establishes a persistent connection with the server. 
 * If the connection is broken, the client attempts to reconnect 
 * when calling the sendRequest method.
 * 
 * Using the static mathod newHttpClient(...) to get shared_ptr of the object implementing 
 * the class, the shared_ptr is retained in the framework until all response callbacks 
 * are invoked without fear of accidental deconstruction.
 * 
 * TODO:SSL server verification
 */
class HttpClient : public trantor::NonCopyable
{
  public:
    /// Send a request asynchronously to the server
    /**
     * The response from the http server is obtained 
     * in the callback function.
     */
    virtual void sendRequest(const HttpRequestPtr &req, const HttpReqCallback &callback) = 0;
    virtual void sendRequest(const HttpRequestPtr &req, HttpReqCallback &&callback) = 0;

    /// Set the pipelining depth, which is the number of requests that are not responding.
    /**
     * If this method is not called, the default depth value is 0 which means the pipelining is disabled. 
     * For details about pipelining, see rfc2616-8.1.2.2
     */
    virtual void setPipeliningDepth(size_t depth) = 0;

    /// Use ip and port to connect to server
    /** 
     * If useSSL is set to true, the client 
     * connects to the server using https.
     * 
     * If the loop parameter is set to nullptr, the client 
     * uses the HttpAppFramework's event loop, otherwise it
     * runs in the loop identified by the parameter.
     * 
     * Note: The @param ip support for both ipv4 and ipv6 address
     */
    static HttpClientPtr newHttpClient(const std::string &ip,
                                       uint16_t port,
                                       bool useSSL = false,
                                       trantor::EventLoop *loop = nullptr);

    /// Get the event loop of the client;
    virtual trantor::EventLoop *loop() = 0;

    /// Use hostString to connect to server
    /** 
     * Examples for hostString:
     * https://www.baidu.com
     * http://www.baidu.com
     * https://127.0.0.1:8080/
     * http://127.0.0.1
     * http://[::1]:8080/   //IPv6 address must be enclosed in [], rfc2732
     * 
     * The @param hostString must be prefixed by 'http://' or 'https://' 
     * 
     * If the loop parameter is set to nullptr, the client 
     * uses the HttpAppFramework's event loop, otherwise it
     * runs in the loop identified by the parameter.
     * 
     * NOTE:
     * Don't add path and parameters in hostString, the request path 
     * and parameters should be set in HttpRequestPtr when calling 
     * the sendRequest() method.
     * 
     */
    static HttpClientPtr newHttpClient(const std::string &hostString,
                                       trantor::EventLoop *loop = nullptr);

    virtual ~HttpClient() {}

  protected:
    HttpClient() = default;
};

} // namespace drogon
