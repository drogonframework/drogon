/**
 *
 *  @file HttpClientImpl.cc
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

#include "HttpClientImpl.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpResponseParser.h"

#include <drogon/config.h>
#include <stdlib.h>
#include <algorithm>
#include <utility>

using namespace trantor;
using namespace drogon;
using namespace std::placeholders;

namespace trantor
{
static const size_t kDefaultDNSTimeout{600};
}

void HttpClientImpl::createTcpClient()
{
    LOG_TRACE << "New TcpClient," << serverAddr_.toIpPort();
    tcpClientPtr_ =
        std::make_shared<trantor::TcpClient>(loop_, serverAddr_, "httpClient");

    if (useSSL_ && utils::supportsTls())
    {
        LOG_TRACE << "useOldTLS=" << useOldTLS_;
        LOG_TRACE << "domain=" << domain_;
        auto policy = trantor::TLSPolicy::defaultClientPolicy();
        policy->setUseOldTLS(useOldTLS_)
            .setValidate(validateCert_)
            .setHostname(domain_)
            .setConfCmds(sslConfCmds_)
            .setCertPath(clientCertPath_)
            .setKeyPath(clientKeyPath_);
        tcpClientPtr_->enableSSL(std::move(policy));
    }

    auto thisPtr = shared_from_this();
    std::weak_ptr<HttpClientImpl> weakPtr = thisPtr;
    tcpClientPtr_->setSockOptCallback([weakPtr](int fd) {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        if (thisPtr->sockOptCallback_)
            thisPtr->sockOptCallback_(fd);
    });
    tcpClientPtr_->setConnectionCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            if (connPtr->connected())
            {
                connPtr->setContext(
                    std::make_shared<HttpResponseParser>(connPtr));
                // send request;
                LOG_TRACE << "Connection established!";
                while (thisPtr->pipeliningCallbacks_.size() <=
                           thisPtr->pipeliningDepth_ &&
                       !thisPtr->requestsBuffer_.empty())
                {
                    thisPtr->sendBufferedRequest(connPtr);
                }
            }
            else
            {
                LOG_TRACE << "connection disconnect";
                auto responseParser = connPtr->getContext<HttpResponseParser>();
                if (responseParser && responseParser->parseResponseOnClose() &&
                    responseParser->gotAll())
                {
                    auto &firstReq = thisPtr->pipeliningCallbacks_.front();
                    if (firstReq->isHead)
                    {
                        responseParser->setForHeadMethod();
                    }
                    auto resp = responseParser->responseImpl();
                    responseParser->reset();
                    // temporary fix of dead tcpClientPtr_
                    // TODO: fix HttpResponseParser when content-length absence
                    thisPtr->tcpClientPtr_.reset();
                    thisPtr->handleResponse(resp, connPtr);
                    if (!thisPtr->requestsBuffer_.empty())
                    {
                        thisPtr->createTcpClient();
                    }
                    return;
                }
                thisPtr->onError(ReqResult::NetworkFailure);
            }
        });
    tcpClientPtr_->setConnectionErrorCallback([weakPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        // can't connect to server
        thisPtr->onError(ReqResult::BadServerAddress);
    });
    tcpClientPtr_->setMessageCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr,
                  trantor::MsgBuffer *msg) {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                thisPtr->onRecvMessage(connPtr, msg);
            }
        });
    tcpClientPtr_->setSSLErrorCallback([weakPtr](SSLError err) {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        if (err == trantor::SSLError::kSSLHandshakeError)
            thisPtr->onError(ReqResult::HandshakeError);
        else if (err == trantor::SSLError::kSSLInvalidCertificate)
            thisPtr->onError(ReqResult::InvalidCertificate);
        else if (err == trantor::SSLError::kSSLProtocolError)
            thisPtr->onError(ReqResult::EncryptionFailure);
        else
        {
            LOG_FATAL << "Invalid value for SSLError";
            abort();
        }
    });
    tcpClientPtr_->connect();
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const trantor::InetAddress &addr,
                               bool useSSL,
                               bool useOldTLS,
                               bool validateCert,
                               Timeouts timeouts)
    : loop_(loop),
      serverAddr_(addr),
      useSSL_(useSSL),
      validateCert_(validateCert),
      timeouts_(timeouts),
      useOldTLS_(useOldTLS)
{
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const std::string &hostString,
                               bool useOldTLS,
                               bool validateCert,
                               Timeouts timeouts)
    : loop_(loop),
      validateCert_(validateCert),
      timeouts_(timeouts),
      useOldTLS_(useOldTLS)
{
    auto lowerHost = hostString;
    std::transform(lowerHost.begin(),
                   lowerHost.end(),
                   lowerHost.begin(),
                   [](unsigned char c) { return tolower(c); });
    if (lowerHost.find("https://") == 0)
    {
        useSSL_ = true;
        lowerHost = lowerHost.substr(8);
    }
    else if (lowerHost.find("http://") == 0)
    {
        useSSL_ = false;
        lowerHost = lowerHost.substr(7);
    }
    else
    {
        return;
    }
    auto pos = lowerHost.find(']');
    if (lowerHost[0] == '[' && pos != std::string::npos)
    {
        // ipv6
        domain_ = lowerHost.substr(1, pos - 1);
        if (lowerHost[pos + 1] == ':')
        {
            auto portStr = lowerHost.substr(pos + 2);
            pos = portStr.find('/');
            if (pos != std::string::npos)
            {
                portStr = portStr.substr(0, pos);
            }
            auto port = atoi(portStr.c_str());
            if (port > 0 && port < 65536)
            {
                serverAddr_ = InetAddress(domain_, port, true);
            }
        }
        else
        {
            if (useSSL_)
            {
                serverAddr_ = InetAddress(domain_, 443, true);
            }
            else
            {
                serverAddr_ = InetAddress(domain_, 80, true);
            }
        }
    }
    else
    {
        auto pos = lowerHost.find(':');
        if (pos != std::string::npos)
        {
            domain_ = lowerHost.substr(0, pos);
            auto portStr = lowerHost.substr(pos + 1);
            pos = portStr.find('/');
            if (pos != std::string::npos)
            {
                portStr = portStr.substr(0, pos);
            }
            auto port = atoi(portStr.c_str());
            if (port > 0 && port < 65536)
            {
                serverAddr_ = InetAddress(domain_, port);
            }
        }
        else
        {
            domain_ = lowerHost;
            pos = domain_.find('/');
            if (pos != std::string::npos)
            {
                domain_ = domain_.substr(0, pos);
            }
            if (useSSL_)
            {
                serverAddr_ = InetAddress(domain_, 443);
            }
            else
            {
                serverAddr_ = InetAddress(domain_, 80);
            }
        }
    }
    if (serverAddr_.isUnspecified())
    {
        isDomainName_ = true;
    }
    LOG_TRACE << "userSSL=" << useSSL_ << " domain=" << domain_;
}

HttpClientImpl::~HttpClientImpl()
{
    LOG_TRACE << "Deconstruction HttpClient";
    if (resolverPtr_ && !(loop_->isInLoopThread()))
    {
        // Make sure the resolverPtr_ is destroyed in the correct thread.
        loop_->queueInLoop([resolverPtr = std::move(resolverPtr_)]() {});
    }
}

void HttpClientImpl::sendRequest(const drogon::HttpRequestPtr &req,
                                 const drogon::HttpReqCallback &callback,
                                 double timeout)
{
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr, req, callback = callback, timeout]() mutable {
        thisPtr->sendRequestInLoop(req, std::move(callback), timeout);
    });
}

void HttpClientImpl::sendRequest(const drogon::HttpRequestPtr &req,
                                 drogon::HttpReqCallback &&callback,
                                 double timeout)
{
    auto thisPtr = shared_from_this();
    loop_->runInLoop(
        [thisPtr, req, callback = std::move(callback), timeout]() mutable {
            thisPtr->sendRequestInLoop(req, std::move(callback), timeout);
        });
}

void HttpClientImpl::PendingRequest::complete(ReqResult result,
                                              const HttpResponsePtr &response)
{
    if (!client)
        return;
    // Release both owners even when this slot must wait for a late response.
    auto keepAlive = std::move(client);
    auto cb = std::exchange(callback, {});
    cb(result, response);
}

void HttpClientImpl::startTimeout(const PendingRequestPtr &pending,
                                  double seconds,
                                  ReqResult result)
{
    if (seconds <= 0)
        return;

    loop_->runAfter(seconds,
                    [weak = std::weak_ptr<PendingRequest>(pending), result]() {
                        auto pending = weak.lock();
                        if (!pending || !pending->client)
                            return;
                        auto &client = *pending->client;
                        if (pending->bufferPosition !=
                            client.requestsBuffer_.end())
                        {
                            client.failBufferedRequest(pending, result);
                        }
                        else if (result != ReqResult::ConnectTimeout)
                        {
                            // Keep the FIFO slot for a late response to this
                            // submission.
                            pending->complete(result, nullptr);
                        }
                    });
}

void HttpClientImpl::enqueueRequest(const PendingRequestPtr &pending)
{
    pending->bufferPosition =
        requestsBuffer_.insert(requestsBuffer_.end(), pending);
    requestsBufferSize_.fetch_add(1, std::memory_order_relaxed);
    startTimeout(pending, timeouts_.connectTimeout, ReqResult::ConnectTimeout);
}

void HttpClientImpl::removeBufferedRequest(const PendingRequestPtr &pending)
{
    requestsBuffer_.erase(pending->bufferPosition);
    pending->bufferPosition = requestsBuffer_.end();
    requestsBufferSize_.fetch_sub(1, std::memory_order_relaxed);
}

void HttpClientImpl::failBufferedRequest(PendingRequestPtr pending,
                                         ReqResult result)
{
    removeBufferedRequest(pending);
    outstandingRequests_.fetch_sub(1, std::memory_order_relaxed);
    pending->complete(result, nullptr);
}

static bool isValidIpAddr(const trantor::InetAddress &addr)
{
    if (addr.portNetEndian() == 0)
    {
        return false;
    }
    if (!addr.isIpV6())
    {
        return addr.ipNetEndian() != 0;
    }
    // Is ipv6
    auto ipaddr = addr.ip6NetEndian();
    for (int i = 0; i < 4; ++i)
    {
        if (ipaddr[i] != 0)
        {
            return true;
        }
    }
    return false;
}

void HttpClientImpl::sendRequestInLoop(const drogon::HttpRequestPtr &req,
                                       drogon::HttpReqCallback &&callback,
                                       double timeout)
{
    outstandingRequests_.fetch_add(1, std::memory_order_relaxed);

    loop_->assertInLoopThread();
    auto pending = std::make_shared<PendingRequest>(PendingRequest{
        req, std::move(callback), shared_from_this(), requestsBuffer_.end()});
    startTimeout(pending, timeout, ReqResult::Timeout);
    for (const auto &header : headers_)
    {
        if (req->headers().find(header.first) == req->headers().end())
        {
            req->addHeader(header.first, header.second);
        }
    }
    if (!static_cast<drogon::HttpRequestImpl *>(req.get())->passThrough())
    {
        if (req->getHeader("connection").empty())
            req->addHeader("connection", "Keep-Alive");
        if (!userAgent_.empty() && req->getHeader("user-agent").empty())
            req->addHeader("user-agent", userAgent_);
    }
    // Set the host header if not already set
    if (req->getHeader("host").empty())
    {
        if (onDefaultPort())
        {
            req->addHeader("host", host());
        }
        else
        {
            req->addHeader("host", host() + ":" + std::to_string(port()));
        }
    }

    for (auto &cookie : validCookies_)
    {
        if ((cookie.expiresDate().microSecondsSinceEpoch() == 0 ||
             cookie.expiresDate() > trantor::Date::now()) &&
            (cookie.path().empty() || req->path().find(cookie.path()) == 0))
        {
            req->addCookie(cookie.key(), cookie.value());
        }
    }

    if (!tcpClientPtr_)
    {
        enqueueRequest(pending);
        if (domain_.empty() || !isDomainName_)
        {
            // Valid ip address, no domain, connect directly
            if (isValidIpAddr(serverAddr_))
            {
                createTcpClient();
            }
            // No ip address and no domain, respond with BadServerAddress
            else
            {
                failBufferedRequest(pending, ReqResult::BadServerAddress);
            }
            return;
        }

        // A dns query is on going.
        if (dns_)
        {
            return;
        }

        // Always do dns query when (re)connects a domain.
        dns_ = true;
        if (!resolverPtr_)
        {
            resolverPtr_ =
                trantor::Resolver::newResolver(loop_, kDefaultDNSTimeout);
        }
        auto thisPtr = shared_from_this();
        resolverPtr_->resolve(
            domain_, [thisPtr](const trantor::InetAddress &addr) {
                thisPtr->loop_->runInLoop([thisPtr, addr]() {
                    // Retrieve port from old serverAddr_
                    auto port = thisPtr->serverAddr_.portNetEndian();
                    thisPtr->serverAddr_ = addr;
                    thisPtr->serverAddr_.setPortNetEndian(port);
                    LOG_TRACE << "dns:domain=" << thisPtr->domain_
                              << ";ip=" << thisPtr->serverAddr_.toIp();
                    thisPtr->dns_ = false;

                    if (isValidIpAddr(thisPtr->serverAddr_))
                    {
                        thisPtr->createTcpClient();
                        return;
                    }

                    // DNS fail to get valid ip address,
                    // respond all requests with BadServerAddress
                    while (!thisPtr->requestsBuffer_.empty())
                    {
                        thisPtr->failBufferedRequest(
                            thisPtr->requestsBuffer_.front(),
                            ReqResult::BadServerAddress);
                    }
                });
            });
        return;
    }

    auto connPtr = tcpClientPtr_->connection();
    // Trantor can expose the TCP connection while TLS is still negotiating.
    // The response parser is installed once the connection is ready for HTTP.
    if (connPtr && connPtr->connected() && connPtr->hasContext() &&
        requestsBuffer_.empty() &&
        pipeliningCallbacks_.size() <= pipeliningDepth_)
    {
        sendReq(connPtr, pending);
    }
    else
    {
        enqueueRequest(pending);
    }
}

void HttpClientImpl::sendBufferedRequest(
    const trantor::TcpConnectionPtr &connPtr)
{
    auto pending = requestsBuffer_.front();
    removeBufferedRequest(pending);
    sendReq(connPtr, pending);
}

void HttpClientImpl::sendReq(const trantor::TcpConnectionPtr &connPtr,
                             const PendingRequestPtr &pending)
{
    trantor::MsgBuffer buffer;
    auto implPtr = static_cast<HttpRequestImpl *>(pending->request.get());
    implPtr->appendToBuffer(&buffer);
    LOG_TRACE << "Send request:"
              << std::string(buffer.peek(), buffer.readableBytes());
    bytesSent_ += buffer.readableBytes();
    // The caller may reuse and change the request after a timeout.
    pending->isHead = implPtr->method() == Head;
    pipeliningCallbacks_.push(pending);
    startTimeout(pending,
                 timeouts_.responseTimeout,
                 ReqResult::ResponseTimeout);
    connPtr->send(std::move(buffer));
}

void HttpClientImpl::handleResponse(const HttpResponseImplPtr &resp,
                                    const trantor::TcpConnectionPtr &connPtr)
{
    assert(!pipeliningCallbacks_.empty());
    auto &coding = resp->getHeaderBy("content-encoding");
    if (coding == "gzip")
    {
        resp->gunzip();
    }
#ifdef USE_BROTLI
    else if (coding == "br")
    {
        resp->brDecompress();
    }
#endif
    handleCookies(resp);
    completeSentRequest(ReqResult::Ok, resp);

    // LOG_TRACE << "pipelining buffer size=" <<
    // pipeliningCallbacks_.size(); LOG_TRACE << "requests buffer size="
    // << requestsBuffer_.size();

    if (connPtr->connected())
    {
        if (!requestsBuffer_.empty() &&
            pipeliningCallbacks_.size() <= pipeliningDepth_)
        {
            sendBufferedRequest(connPtr);
        }
        else if (resp->ifCloseConnection() && pipeliningCallbacks_.empty())
        {
            tcpClientPtr_.reset();
        }
    }
    else
    {
        while (!pipeliningCallbacks_.empty())
        {
            completeSentRequest(ReqResult::NetworkFailure, nullptr);
        }
    }
}

void HttpClientImpl::onRecvMessage(const trantor::TcpConnectionPtr &connPtr,
                                   trantor::MsgBuffer *msg)
{
    auto responseParser = connPtr->getContext<HttpResponseParser>();

    // LOG_TRACE << "###:" << msg->readableBytes();
    auto msgSize = msg->readableBytes();
    while (msg->readableBytes() > 0)
    {
        if (pipeliningCallbacks_.empty())
        {
            LOG_ERROR << "More responses than expected!";
            connPtr->shutdown();
            return;
        }
        auto &firstReq = pipeliningCallbacks_.front();
        if (firstReq->isHead)
        {
            responseParser->setForHeadMethod();
        }
        if (!responseParser->parseResponse(msg))
        {
            onError(ReqResult::BadResponse);
            bytesReceived_ += (msgSize - msg->readableBytes());
            return;
        }
        if (responseParser->gotAll())
        {
            auto resp = responseParser->responseImpl();
            resp->setPeerCertificate(connPtr->peerCertificate());
            responseParser->reset();
            bytesReceived_ += (msgSize - msg->readableBytes());
            msgSize = msg->readableBytes();
            const auto code = static_cast<int>(resp->statusCode());
            if (code >= 100 && code < 200 && code != k101SwitchingProtocols)
            {
                // Informational response, not the final one. Keep waiting for
                // the response that actually belongs to the request.
                continue;
            }
            handleResponse(resp, connPtr);
        }
        else
        {
            bytesReceived_ += (msgSize - msg->readableBytes());
            break;
        }
    }
}

HttpClientPtr HttpClient::newHttpClient(const std::string &ip,
                                        uint16_t port,
                                        bool useSSL,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert)
{
    return newHttpClient(
        ip, port, {0, 0}, useSSL, loop, useOldTLS, validateCert);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &ip,
                                        uint16_t port,
                                        Timeouts timeouts,
                                        bool useSSL,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert)
{
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL,
        useOldTLS,
        validateCert,
        timeouts);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &hostString,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert)
{
    return newHttpClient(hostString, {0, 0}, loop, useOldTLS, validateCert);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &hostString,
                                        Timeouts timeouts,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert)
{
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        useOldTLS,
        validateCert,
        timeouts);
}

void HttpClientImpl::completeSentRequest(ReqResult result,
                                         const HttpResponsePtr &response)
{
    auto pending = std::move(pipeliningCallbacks_.front());
    pipeliningCallbacks_.pop();
    outstandingRequests_.fetch_sub(1, std::memory_order_relaxed);
    pending->complete(result, response);
}

void HttpClientImpl::onError(ReqResult result)
{
    while (!pipeliningCallbacks_.empty())
    {
        completeSentRequest(result, nullptr);
    }
    while (!requestsBuffer_.empty())
    {
        failBufferedRequest(requestsBuffer_.front(), result);
    }
    tcpClientPtr_.reset();
}

void HttpClientImpl::handleCookies(const HttpResponseImplPtr &resp)
{
    loop_->assertInLoopThread();
    if (!enableCookies_)
        return;
    for (auto &iter : resp->getCookies())
    {
        auto &cookie = iter.second;
        if (!cookie.domain().empty() && cookie.domain() != domain_)
        {
            continue;
        }
        if (cookie.isSecure())
        {
            if (useSSL_)
            {
                validCookies_.emplace_back(cookie);
            }
        }
        else
        {
            validCookies_.emplace_back(cookie);
        }
    }
}

void HttpClientImpl::setCertPath(const std::string &cert,
                                 const std::string &key)
{
    clientCertPath_ = cert;
    clientKeyPath_ = key;
}

void HttpClientImpl::addSSLConfigs(
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
    for (const auto &cmd : sslConfCmds)
    {
        sslConfCmds_.push_back(cmd);
    }
}
