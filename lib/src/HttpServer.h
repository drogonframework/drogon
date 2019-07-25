/**
 *
 *  HttpServer.h
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

#include "HttpRequestImpl.h"
#include "WebSocketConnectionImpl.h"
#include <drogon/WebSocketController.h>
#include <drogon/config.h>
#include <trantor/net/TcpServer.h>
#include <trantor/net/callbacks.h>
#include <trantor/utils/NonCopyable.h>

#include <functional>
#include <string>
#include <vector>

namespace drogon
{
class HttpRequest;
class HttpResponse;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
typedef std::function<void(const HttpRequestImplPtr &,
                           std::function<void(const HttpResponsePtr &)> &&)>
    HttpAsyncCallback;
typedef std::function<void(const HttpRequestImplPtr &,
                           std::function<void(const HttpResponsePtr &)> &&,
                           const WebSocketConnectionImplPtr &)>
    WebSocketNewAsyncCallback;
class HttpServer : trantor::NonCopyable
{
  public:
    HttpServer(trantor::EventLoop *loop,
               const trantor::InetAddress &listenAddr,
               const std::string &name);

    ~HttpServer();

    trantor::EventLoop *getLoop() const
    {
        return _server.getLoop();
    }

    void setHttpAsyncCallback(const HttpAsyncCallback &cb)
    {
        _httpAsyncCallback = cb;
    }
    void setNewWebsocketCallback(const WebSocketNewAsyncCallback &cb)
    {
        _newWebsocketCallback = cb;
    }
    void setConnectionCallback(const trantor::ConnectionCallback &cb)
    {
        _connectionCallback = cb;
    }
    void setIoLoopThreadPool(
        const std::shared_ptr<trantor::EventLoopThreadPool> &pool)
    {
        _server.setIoLoopThreadPool(pool);
    }
    void setIoLoopNum(int numThreads)
    {
        _server.setIoLoopNum(numThreads);
    }
    void kickoffIdleConnections(size_t timeout)
    {
        _server.kickoffIdleConnections(timeout);
    }
    trantor::EventLoop *getLoop()
    {
        return _server.getLoop();
    }
    std::vector<trantor::EventLoop *> getIoLoops()
    {
        return _server.getIoLoops();
    }
    void start();

#ifdef USE_OPENSSL
    void enableSSL(const std::string &certPath, const std::string &keyPath)
    {
        _server.enableSSL(certPath, keyPath);
    }
#endif

  private:
    void onConnection(const trantor::TcpConnectionPtr &conn);
    void onMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onRequests(const trantor::TcpConnectionPtr &,
                    const std::vector<HttpRequestImplPtr> &);
    void sendResponse(const trantor::TcpConnectionPtr &,
                      const HttpResponsePtr &,
                      bool isHeadMethod);
    void sendResponses(
        const trantor::TcpConnectionPtr &conn,
        const std::vector<std::pair<HttpResponsePtr, bool>> &responses);
    trantor::TcpServer _server;
    HttpAsyncCallback _httpAsyncCallback;
    WebSocketNewAsyncCallback _newWebsocketCallback;
    trantor::ConnectionCallback _connectionCallback;
};

}  // namespace drogon
