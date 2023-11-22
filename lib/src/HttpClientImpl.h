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

#include <drogon/Cookie.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/Resolver.h>
#include <trantor/net/TcpClient.h>
#include <list>
#include <mutex>
#include <queue>
#include <vector>
#include "impl_forwards.h"
#include "Http2Transport.h"
#include "HttpTransport.h"

namespace drogon
{

class Http1xTransport : public HttpTransport
{
  private:
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> pipeliningCallbacks_;
    trantor::TcpConnectionPtr connPtr;
    size_t *bytesSent_;
    size_t *bytesReceived_;

    void sendReq(const HttpRequestPtr &req);

  public:
    Http1xTransport(trantor::TcpConnectionPtr connPtr,
                    size_t *bytesSent,
                    size_t *bytesReceived);
    virtual ~Http1xTransport();
    void sendRequestInLoop(const HttpRequestPtr &req,
                           HttpReqCallback &&callback) override;
    void onRecvMessage(const trantor::TcpConnectionPtr &,
                       trantor::MsgBuffer *) override;

    size_t requestsInFlight() const override
    {
        return pipeliningCallbacks_.size();
    }

    bool handleConnectionClose() override;

    void onError(ReqResult result) override
    {
        while (!pipeliningCallbacks_.empty())
        {
            auto &cb = pipeliningCallbacks_.front().second;
            cb(result, nullptr);
            pipeliningCallbacks_.pop();
        }
    }
};

class HttpClientImpl final : public HttpClient,
                             public std::enable_shared_from_this<HttpClientImpl>
{
  public:
    HttpClientImpl(trantor::EventLoop *loop,
                   const trantor::InetAddress &addr,
                   bool useSSL = false,
                   bool useOldTLS = false,
                   bool validateCert = true,
                   std::optional<Version> httpVersion = std::nullopt);
    HttpClientImpl(trantor::EventLoop *loop,
                   const std::string &hostString,
                   bool useOldTLS = false,
                   bool validateCert = true,
                   std::optional<Version> httpVersion = std::nullopt);
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

    void setSockOptCallback(std::function<void(int)> cb) override
    {
        sockOptCallback_ = std::move(cb);
    }

    std::optional<Version> protocolVersion() const override
    {
        return httpVersion_;
    }

  private:
    std::shared_ptr<trantor::TcpClient> tcpClientPtr_;
    trantor::EventLoop *loop_;
    trantor::InetAddress serverAddr_;
    bool useSSL_;
    bool validateCert_;
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
    std::list<std::pair<HttpRequestPtr, HttpReqCallback>> requestsBuffer_;
    void onRecvMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onError(ReqResult result);
    std::string domain_;
    bool isDomainName_{true};  // true if domain_ is name
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
    std::function<void(int)> sockOptCallback_;
    std::unique_ptr<HttpTransport> transport_;
    std::optional<Version> targetHttpVersion_;
    std::optional<Version> httpVersion_;
};

using HttpClientImplPtr = std::shared_ptr<HttpClientImpl>;
}  // namespace drogon
