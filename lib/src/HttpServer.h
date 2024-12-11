/**
 *
 *  @file HttpServer.h
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

#include <trantor/net/TcpServer.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <string>
#include <vector>
#include "impl_forwards.h"

struct CallbackParamPack;

namespace drogon
{
struct ControllerBinderBase;

class HttpServer : trantor::NonCopyable
{
  public:
    HttpServer(trantor::EventLoop *loop,
               const trantor::InetAddress &listenAddr,
               std::string name);

    ~HttpServer();

    void setIoLoops(const std::vector<trantor::EventLoop *> &ioLoops)
    {
        server_.setIoLoops(ioLoops);
    }

    void start();
    void stop();

    void enableSSL(trantor::TLSPolicyPtr policy)
    {
        server_.enableSSL(std::move(policy));
    }

    void reloadSSL()
    {
        server_.reloadSSL();
    }

    const trantor::InetAddress &address() const
    {
        return server_.address();
    }

    void setBeforeListenSockOptCallback(std::function<void(int)> cb)
    {
        beforeListenSetSockOptCallback_ = std::move(cb);
    }

    void setAfterAcceptSockOptCallback(std::function<void(int)> cb)
    {
        afterAcceptSetSockOptCallback_ = std::move(cb);
    }

    void setConnectionCallback(
        std::function<void(const trantor::TcpConnectionPtr &)> cb)
    {
        connectionCallback_ = std::move(cb);
    }

  private:
    friend class HttpInternalForwardHelper;

    static void onConnection(const trantor::TcpConnectionPtr &conn);
    static void onMessage(const trantor::TcpConnectionPtr &,
                          trantor::MsgBuffer *);
    static void onRequests(const trantor::TcpConnectionPtr &,
                           const std::vector<HttpRequestImplPtr> &,
                           const std::shared_ptr<HttpRequestParser> &);

    struct HttpRequestParamPack
    {
        std::shared_ptr<ControllerBinderBase> binderPtr;
        std::function<void(const HttpResponsePtr &)> callback;
    };

    struct WsRequestParamPack
    {
        std::shared_ptr<ControllerBinderBase> binderPtr;
        std::function<void(const HttpResponsePtr &)> callback;
        WebSocketConnectionImplPtr wsConnPtr;
    };

    // Http request handling steps
    static void onHttpRequest(const HttpRequestImplPtr &,
                              std::function<void(const HttpResponsePtr &)> &&);
    static void httpRequestRouting(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    static void httpRequestHandling(
        const HttpRequestImplPtr &req,
        std::shared_ptr<ControllerBinderBase> &&binderPtr,
        std::function<void(const HttpResponsePtr &)> &&callback);

    // Websocket request handling steps
    static void onWebsocketRequest(
        const HttpRequestImplPtr &,
        std::function<void(const HttpResponsePtr &)> &&,
        WebSocketConnectionImplPtr &&);
    static void websocketRequestRouting(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        WebSocketConnectionImplPtr &&wsConnPtr);
    static void websocketRequestHandling(
        const HttpRequestImplPtr &req,
        std::shared_ptr<ControllerBinderBase> &&binderPtr,
        std::function<void(const HttpResponsePtr &)> &&callback,
        WebSocketConnectionImplPtr &&wsConnPtr);

    // Http/Websocket shared handling steps
    template <typename Pack>
    static void requestPostRouting(const HttpRequestImplPtr &req, Pack &&pack);
    template <typename Pack>
    static void requestPassMiddlewares(const HttpRequestImplPtr &req,
                                       Pack &&pack);
    template <typename Pack>
    static void requestPreHandling(const HttpRequestImplPtr &req, Pack &&pack);

    // Response buffering and sending
    static void handleResponse(
        const HttpResponsePtr &response,
        const std::shared_ptr<CallbackParamPack> &paramPack,
        bool *respReadyPtr);
    static void sendResponse(const trantor::TcpConnectionPtr &,
                             const HttpResponsePtr &,
                             bool isHeadMethod);
    static void sendResponses(
        const trantor::TcpConnectionPtr &conn,
        const std::vector<std::pair<HttpResponsePtr, bool>> &responses,
        trantor::MsgBuffer &buffer);

    trantor::TcpServer server_;

    std::function<void(int)> beforeListenSetSockOptCallback_;
    std::function<void(int)> afterAcceptSetSockOptCallback_;
    std::function<void(const trantor::TcpConnectionPtr &)> connectionCallback_;
};

class HttpInternalForwardHelper
{
  public:
    static void forward(const HttpRequestImplPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
    {
        return HttpServer::onHttpRequest(req, std::move(callback));
    }
};

}  // namespace drogon
