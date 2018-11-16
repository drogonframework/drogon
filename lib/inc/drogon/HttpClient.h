/**
 *
 *  HttpClient.h
 * 
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <memory>
namespace drogon
{
enum class ReqResult
{
  Ok,
  BadResponse,
  NetworkFailure,
  BadServerAddress,
  Timeout
};
typedef std::function<void(ReqResult, const HttpResponsePtr &response)> HttpReqCallback;
class HttpClient;
typedef std::shared_ptr<HttpClient> HttpClientPtr;

///Async http client class
/**
 * HttpClient implementation object uses the HttpAppFramework's event loop,
 * so you should call app().run() to make the client work.
 * Each HttpClient object establishes a persistent connection with the server. 
 * If the connection is broken, the client will attempt to reconnect 
 * when calling the sendRequest method.
 */
class HttpClient : public trantor::NonCopyable
{
public:
  /// Send request to server
  /**
   * The response from http server will be got in
   * the callback function
   */
  virtual void sendRequest(const HttpRequestPtr &req, const HttpReqCallback &callback) = 0;

  virtual ~HttpClient() {}

  /// Use ip and port to connect to server
  /** If useSSL is set to true, the client will 
   * connect to the server using https
   */
  static HttpClientPtr newHttpClient(const std::string &ip, uint16_t port, bool useSSL = false);

  /// Use hostString to connect to server
  /** Examples for hostString:
   *  https://www.baidu.com
   *  http://www.baidu.com
   *  https://127.0.0.1:8080/
   *  http://127.0.0.1
   *  Note:don't add path and parameters in hostString, the request path 
   *  and parameters should be set in 
   *  HttpRequestPtr 
   */
  static HttpClientPtr newHttpClient(const std::string &hostString);

protected:
  HttpClient() = default;
};
} // namespace drogon