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

#include "WebSockectConnectionImpl.h"
#include "HttpRequestImpl.h"
#include <drogon/config.h>
#include <trantor/net/TcpServer.h>
#include <trantor/net/callbacks.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <string>

using namespace trantor;
namespace drogon
{
class HttpRequest;
class HttpResponse;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
class HttpServer : trantor::NonCopyable
{
  public:
    typedef std::function<void(const HttpRequestImplPtr &, const std::function<void(const HttpResponsePtr &)> &)> HttpAsyncCallback;
    typedef std::function<void(const HttpRequestImplPtr &,
                               const std::function<void(const HttpResponsePtr &)> &,
                               const WebSocketConnectionPtr &)>
        WebSocketNewAsyncCallback;
    typedef std::function<void(const WebSocketConnectionPtr &)>
        WebSocketDisconnetCallback;
    typedef std::function<void(const WebSocketConnectionPtr &, trantor::MsgBuffer *)>
        WebSocketMessageCallback;

    HttpServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const std::string &name);

    ~HttpServer();

    EventLoop *getLoop() const { return _server.getLoop(); }

    void setHttpAsyncCallback(const HttpAsyncCallback &cb)
    {
        _httpAsyncCallback = cb;
    }
    void setNewWebsocketCallback(const WebSocketNewAsyncCallback &cb)
    {
        _newWebsocketCallback = cb;
    }
    void setDisconnectWebsocketCallback(const WebSocketDisconnetCallback &cb)
    {
        _disconnectWebsocketCallback = cb;
    }
    void setWebsocketMessageCallback(const WebSocketMessageCallback &cb)
    {
        _webSocketMessageCallback = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        _connectionCallback = cb;
    }
    void setIoLoopNum(int numThreads)
    {
        _server.setIoLoopNum(numThreads);
    }
    void kickoffIdleConnections(size_t timeout)
    {
        _server.kickoffIdleConnections(timeout);
    }
    void start();

#ifdef USE_OPENSSL
    void enableSSL(const std::string &certPath, const std::string &keyPath)
    {
        _server.enableSSL(certPath, keyPath);
    }
#endif

  private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &,
                   MsgBuffer *);
    void onRequest(const TcpConnectionPtr &, const HttpRequestImplPtr &);
    bool isWebSocket(const TcpConnectionPtr &conn, const HttpRequestImplPtr &req);
    void sendResponse(const TcpConnectionPtr &, const HttpResponsePtr &);
    trantor::TcpServer _server;
    HttpAsyncCallback _httpAsyncCallback;
    WebSocketNewAsyncCallback _newWebsocketCallback;
    WebSocketDisconnetCallback _disconnectWebsocketCallback;
    WebSocketMessageCallback _webSocketMessageCallback;
    trantor::ConnectionCallback _connectionCallback;
};

} // namespace drogon
