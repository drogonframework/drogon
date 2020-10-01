/**
 *
 *  @file HttpClientImpl.h
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

#include "impl_forwards.h"
#include <drogon/HttpClient.h>
#include <drogon/Cookie.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/Resolver.h>
#include <mutex>
#include <queue>
#include <vector>

namespace drogon
{
class HttpClientImpl : public HttpClient,
                       public std::enable_shared_from_this<HttpClientImpl>
{
  public:
    HttpClientImpl(trantor::EventLoop *loop,
                   const trantor::InetAddress &addr,
                   bool useSSL = false,
                   bool useOldTLS = false);
    HttpClientImpl(trantor::EventLoop *loop,
                   const std::string &hostString,
                   bool useOldTLS = false);
    virtual void sendRequest(const HttpRequestPtr &req,
                             const HttpReqCallback &callback,
                             double timeout = 0) override;
    virtual void sendRequest(const HttpRequestPtr &req,
                             HttpReqCallback &&callback,
                             double timeout = 0) override;
    virtual trantor::EventLoop *getLoop() override
    {
        return loop_;
    }
    virtual void setPipeliningDepth(size_t depth) override
    {
        pipeliningDepth_ = depth;
    }
    ~HttpClientImpl();

    virtual void enableCookies(bool flag = true) override
    {
        enableCookies_ = flag;
    }

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        validCookies_.emplace_back(Cookie(key, value));
    }

    virtual void addCookie(const Cookie &cookie) override
    {
        validCookies_.emplace_back(cookie);
    }

    virtual size_t bytesSent() const override
    {
        return bytesSent_;
    }
    virtual size_t bytesReceived() const override
    {
        return bytesReceived_;
    }

  private:
    std::shared_ptr<trantor::TcpClient> tcpClientPtr_;
    trantor::EventLoop *loop_;
    trantor::InetAddress serverAddr_;
    bool useSSL_;
    void sendReq(const trantor::TcpConnectionPtr &connPtr,
                 const HttpRequestPtr &req);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback,
                           double timeout);
    void handleCookies(const HttpResponseImplPtr &resp);
    void createTcpClient();
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> pipeliningCallbacks_;
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> requestsBuffer_;
    void onRecvMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onError(ReqResult result);
    std::string domain_;
    size_t pipeliningDepth_{0};
    bool enableCookies_{false};
    std::vector<Cookie> validCookies_;
    size_t bytesSent_{0};
    size_t bytesReceived_{0};
    bool dns_{false};
    std::shared_ptr<trantor::Resolver> resolverPtr_;
    bool useOldTLS_{false};
};
using HttpClientImplPtr = std::shared_ptr<HttpClientImpl>;
}  // namespace drogon
