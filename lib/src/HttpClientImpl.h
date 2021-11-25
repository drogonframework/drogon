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
#include <list>
#include <vector>

namespace drogon
{
class HttpClientImpl final : public HttpClient,
                             public std::enable_shared_from_this<HttpClientImpl>
{
  public:
    HttpClientImpl(trantor::EventLoop *loop,
                   const trantor::InetAddress &addr,
                   bool useSSL = false,
                   bool useOldTLS = false,
                   bool validateCert = true);
    HttpClientImpl(trantor::EventLoop *loop,
                   const std::string &hostString,
                   bool useOldTLS = false,
                   bool validateCert = true);
    void sendRequest(const HttpRequestPtr &req,
                     const HttpReqCallback &callback,
                     double timeout = 0) override;
    void sendRequest(const HttpRequestPtr &req,
                     HttpReqCallback &&callback,
                     double timeout = 0) override;
    trantor::EventLoop *getLoop() override
    {
        return loop_;
    }
    void setPipeliningDepth(size_t depth) override
    {
        pipeliningDepth_ = depth;
    }
    ~HttpClientImpl();

    void enableCookies(bool flag = true) override
    {
        enableCookies_ = flag;
    }

    void addCookie(const std::string &key, const std::string &value) override
    {
        validCookies_.emplace_back(Cookie(key, value));
    }

    void addCookie(const Cookie &cookie) override
    {
        validCookies_.emplace_back(cookie);
    }

    size_t bytesSent() const override
    {
        return bytesSent_;
    }
    size_t bytesReceived() const override
    {
        return bytesReceived_;
    }
    void setUserAgent(const std::string &userAgent) override
    {
        userAgent_ = userAgent;
    }

    uint16_t port() const override
    {
        return serverAddr_.toPort();
    }

    std::string host() const override
    {
        if (domain_.empty())
            return serverAddr_.toIp();
        return domain_;
    }

    bool secure() const override
    {
        return useSSL_;
    }

    void setCertPath(const std::string &cert, const std::string &key) override;
    void addSSLConfigs(const std::vector<std::pair<std::string, std::string>>
                           &sslConfCmds) override;

  private:
    std::shared_ptr<trantor::TcpClient> tcpClientPtr_;
    trantor::EventLoop *loop_;
    trantor::InetAddress serverAddr_;
    bool useSSL_;
    bool validateCert_;
    void sendReq(const trantor::TcpConnectionPtr &connPtr,
                 const HttpRequestPtr &req);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback,
                           double timeout);
    void handleCookies(const HttpResponseImplPtr &resp);
    void handleResponse(const HttpResponseImplPtr &resp,
                        std::pair<HttpRequestPtr, HttpReqCallback> &&reqAndCb,
                        const trantor::TcpConnectionPtr &connPtr);
    void createTcpClient();
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> pipeliningCallbacks_;
    std::list<std::pair<HttpRequestPtr, HttpReqCallback>> requestsBuffer_;
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
    std::string userAgent_{"DrogonClient"};
    std::vector<std::pair<std::string, std::string>> sslConfCmds_;
    std::string clientCertPath_;
    std::string clientKeyPath_;
};
using HttpClientImplPtr = std::shared_ptr<HttpClientImpl>;
}  // namespace drogon
