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
#include <trantor/net/callbacks.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <string>
#include <vector>
#include "impl_forwards.h"

struct CallbackParamPack;
namespace drogon
{
class HttpServer : trantor::NonCopyable
{
  public:
    HttpServer(trantor::EventLoop *loop,
               const trantor::InetAddress &listenAddr,
               std::string name,
               const std::vector<
                   std::function<HttpResponsePtr(const HttpRequestPtr &)>>
                   &syncAdvices,
               const std::vector<std::function<void(const HttpRequestPtr &,
                                                    const HttpResponsePtr &)>>
                   &preSendingAdvices);

    ~HttpServer();

    trantor::EventLoop *getLoop() const
    {
        return server_.getLoop();
    }

    void setHttpAsyncCallback(const HttpAsyncCallback &cb)
    {
        httpAsyncCallback_ = cb;
    }
    void setNewWebsocketCallback(const WebSocketNewAsyncCallback &cb)
    {
        newWebsocketCallback_ = cb;
    }
    void setConnectionCallback(const trantor::ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }
    void setIoLoopThreadPool(
        const std::shared_ptr<trantor::EventLoopThreadPool> &pool)
    {
        server_.setIoLoopThreadPool(pool);
    }
    void setIoLoopNum(int numThreads)
    {
        server_.setIoLoopNum(numThreads);
    }
    void setIoLoops(const std::vector<trantor::EventLoop *> &ioLoops)
    {
        server_.setIoLoops(ioLoops);
    }
    void kickoffIdleConnections(size_t timeout)
    {
        server_.kickoffIdleConnections(timeout);
    }
    trantor::EventLoop *getLoop()
    {
        return server_.getLoop();
    }
    std::vector<trantor::EventLoop *> getIoLoops()
    {
        return server_.getIoLoops();
    }
    void start();
    void stop();

    [[deprecated("Use enableSSL(SSLPolicy) instead")]] void enableSSL(
        const std::string &certPath,
        const std::string &keyPath,
        bool useOldTLS,
        const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
    {
        auto policy =
            trantor::TLSPolicy::defaultServerPolicy(certPath, keyPath);
        policy->setConfCmds(sslConfCmds).setUseOldTLS(useOldTLS);
        server_.enableSSL(std::move(policy));
    }

    void enableSSL(trantor::TLSPolicyPtr policy)
    {
        server_.enableSSL(std::move(policy));
    }

    const trantor::InetAddress &address() const
    {
        return server_.address();
    }

  private:
    void onConnection(const trantor::TcpConnectionPtr &conn);
    void onMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onRequests(const trantor::TcpConnectionPtr &,
                    const std::vector<HttpRequestImplPtr> &,
                    const std::shared_ptr<HttpRequestParser> &);
    void handleResponse(const HttpResponsePtr &response,
                        const std::shared_ptr<CallbackParamPack> &paramPack,
                        bool *respReadyPtr);
    void sendResponse(const trantor::TcpConnectionPtr &,
                      const HttpResponsePtr &,
                      bool isHeadMethod);
    void sendResponses(
        const trantor::TcpConnectionPtr &conn,
        const std::vector<std::pair<HttpResponsePtr, bool>> &responses,
        trantor::MsgBuffer &buffer);
    trantor::TcpServer server_;
    HttpAsyncCallback httpAsyncCallback_;
    WebSocketNewAsyncCallback newWebsocketCallback_;
    trantor::ConnectionCallback connectionCallback_;
    const std::vector<std::function<HttpResponsePtr(const HttpRequestPtr &)>>
        &syncAdvices_;
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &preSendingAdvices_;
};

}  // namespace drogon
