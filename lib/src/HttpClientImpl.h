/**
 *
 *  HttpClientImpl.h
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

#include <drogon/HttpClient.h>
#include <mutex>
#include <queue>
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

  private:
    std::shared_ptr<trantor::TcpClient> _tcpClient;
    trantor::EventLoop *_loop;
    trantor::InetAddress _server;
    bool _useSSL;
    void sendReq(const trantor::TcpConnectionPtr &connPtr,
                 const HttpRequestPtr &req);
    void sendRequestInLoop(const HttpRequestPtr &req,
                           const HttpReqCallback &callback);
    std::queue<HttpReqCallback> _pipeliningCallbacks;
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> _requestsBuffer;
    void onRecvMessage(const trantor::TcpConnectionPtr &, trantor::MsgBuffer *);
    void onError(ReqResult result);
    std::string _domain;
    size_t _pipeliningDepth = 0;
};
typedef std::shared_ptr<HttpClientImpl> HttpClientImplPtr;
}  // namespace drogon
