/**
 *
 *  HttpClientImpl.h
 *  An Tao
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

#include "HttpResponseImpl.h"
#include <drogon/HttpClient.h>
#include <drogon/Cookie.h>
#include <mutex>
#include <queue>
#include <vector>
#include <trantor/net/EventLoop.h>
#include <trantor/net/TcpClient.h>

namespace drogon
{
class HttpClientImpl : public HttpClient,
                       public std::enable_shared_from_this<HttpClientImpl>
{
  public:
    HttpClientImpl(trantor::EventLoop *loop,
                   const trantor::InetAddress &addr,
                   bool useSSL = false);
    HttpClientImpl(trantor::EventLoop *loop, const std::string &hostString);
    virtual void sendRequest(const HttpRequestPtr &req,
                             const HttpReqCallback &callback) override;
    virtual void sendRequest(const HttpRequestPtr &req,
                             HttpReqCallback &&callback) override;
    virtual trantor::EventLoop *getLoop() override
    {
        return _loop;
    }
    virtual void setPipeliningDepth(size_t depth) override
    {
        _pipeliningDepth = depth;
    }
    ~HttpClientImpl();

    virtual void enableCookies(bool flag = true) override
    {
        _enableCookies = flag;
    }

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        _validCookies.emplace_back(Cookie(key, value));
    }

    virtual void addCookie(const Cookie &cookie) override
    {
        _validCookies.emplace_back(cookie);
    }

    virtual size_t bytesSent() const override
    {
        return _bytesSent;
    }
    virtual size_t bytesReceived() const override
    {
        return _bytesReceived;
    }

  private:
    std::shared_ptr<trantor::TcpClient> _tcpClient;
    trantor::EventLoop *_loop;
    trantor::InetAddress _server;
    bool _useSSL;
    void sendReq(const trantor::TcpConnectionPtr &connPtr,
                 const HttpRequestPtr &req);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           const HttpReqCallback &callback);
    void handleCookies(const HttpResponseImplPtr &resp);
    std::queue<HttpReqCallback> _pipeliningCallbacks;
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> _requestsBuffer;
    void onRecvMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onError(ReqResult result);
    std::string _domain;
    size_t _pipeliningDepth = 0;
    bool _enableCookies = false;
    std::vector<Cookie> _validCookies;
    size_t _bytesSent = 0;
    size_t _bytesReceived = 0;
};
typedef std::shared_ptr<HttpClientImpl> HttpClientImplPtr;
}  // namespace drogon
