/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include "WebSockectConnectionImpl.h"
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
    typedef std::function<void(const HttpRequestPtr &, const std::function<void(const HttpResponsePtr &)> &)> HttpAsyncCallback;
    typedef std::function<void(const HttpRequestPtr &,
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

    EventLoop *getLoop() const { return server_.getLoop(); }

    void setHttpAsyncCallback(const HttpAsyncCallback &cb)
    {
        httpAsyncCallback_ = cb;
    }
    void setNewWebsocketCallback(const WebSocketNewAsyncCallback &cb)
    {
        newWebsocketCallback_ = cb;
    }
    void setDisconnectWebsocketCallback(const WebSocketDisconnetCallback &cb)
    {
        disconnectWebsocketCallback_ = cb;
    }
    void setWebsocketMessageCallback(const WebSocketMessageCallback &cb)
    {
        webSocketMessageCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        _connectionCallback = cb;
    }
    void setIoLoopNum(int numThreads)
    {
        server_.setIoLoopNum(numThreads);
    }
    void kickoffIdleConnections(size_t timeout)
    {
        server_.kickoffIdleConnections(timeout);
    }
    void start();

#ifdef USE_OPENSSL
    void enableSSL(const std::string &certPath, const std::string &keyPath)
    {
        server_.enableSSL(certPath, keyPath);
    }
#endif

  private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &,
                   MsgBuffer *);
    void onRequest(const TcpConnectionPtr &, const HttpRequestPtr &);
    bool isWebSocket(const TcpConnectionPtr &conn, const HttpRequestPtr &req);
    void sendResponse(const TcpConnectionPtr &, const HttpResponsePtr &);
    trantor::TcpServer server_;
    HttpAsyncCallback httpAsyncCallback_;
    WebSocketNewAsyncCallback newWebsocketCallback_;
    WebSocketDisconnetCallback disconnectWebsocketCallback_;
    WebSocketMessageCallback webSocketMessageCallback_;
    trantor::ConnectionCallback _connectionCallback;
};

} // namespace drogon
