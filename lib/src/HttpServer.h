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
#include <functional>
#include <string>
#include <trantor/net/TcpServer.h>
#include <trantor/net/callbacks.h>
#include <trantor/utils/NonCopyable.h>

using namespace trantor;
namespace drogon
{
class HttpRequest;
class HttpResponse;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
class HttpServer : trantor::NonCopyable
{
  public:
    typedef std::function<void(const HttpRequestImplPtr &, std::function<void(const HttpResponsePtr &)> &&)> HttpAsyncCallback;
    typedef std::function<
        void(const HttpRequestImplPtr &, std::function<void(const HttpResponsePtr &)> &&, const WebSocketConnectionImplPtr &)>
        WebSocketNewAsyncCallback;

    HttpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &name);

    ~HttpServer();

    EventLoop *getLoop() const
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
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &, MsgBuffer *);
    void onRequest(const TcpConnectionPtr &, const HttpRequestImplPtr &);
    void sendResponse(const TcpConnectionPtr &, const HttpResponsePtr &, bool isHeadMethod);
    trantor::TcpServer _server;
    HttpAsyncCallback _httpAsyncCallback;
    WebSocketNewAsyncCallback _newWebsocketCallback;
    trantor::ConnectionCallback _connectionCallback;
};

}  // namespace drogon
