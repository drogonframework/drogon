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

using namespace trantor;
using namespace drogon;
using namespace std::placeholders;

namespace trantor
{
const static size_t kDefaultDNSTimeout{600};
}

void HttpClientImpl::createTcpClient()
{
    LOG_TRACE << "New TcpClient," << serverAddr_.toIpPort();
    tcpClientPtr_ =
        std::make_shared<trantor::TcpClient>(loop_, serverAddr_, "httpClient");
    Version version = targetHttpVersion_.value_or(Version::kHttp2);

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
        if (version == Version::kHttp2)
            policy->setAlpnProtocols({"h2", "http/1.1"});
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
    tcpClientPtr_->setConnectionCallback([weakPtr](
                                             const trantor::TcpConnectionPtr
                                                 &connPtr) {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        if (connPtr->connected())
        {
            // send request;
            LOG_TRACE << "Connection established!";

            auto protocol = connPtr->applicationProtocol();
            if (protocol == "http/1.1")
            {
                LOG_TRACE << "Select http/1.1 protocol";
                thisPtr->transport_ =
                    std::make_unique<Http1xTransport>(connPtr,
                                                      Version::kHttp11,
                                                      &thisPtr->bytesSent_,
                                                      &thisPtr->bytesReceived_);
                thisPtr->httpVersion_ = Version::kHttp11;
            }
            else if (protocol == "h2")
            {
                LOG_TRACE << "Select http/2 protocol";
                thisPtr->transport_ =
                    std::make_unique<Http2Transport>(connPtr,
                                                     &thisPtr->bytesSent_,
                                                     &thisPtr->bytesReceived_);
                thisPtr->httpVersion_ = Version::kHttp2;
            }
            else if (protocol.empty())
            {
                // Either we are not using TLS or the server does not support
                // ALPN. Use HTTP/1.1 if not specified otherwise.
                bool force1_0 = thisPtr->targetHttpVersion_.value_or(
                                    Version::kUnknown) == Version::kHttp10;
                auto version = force1_0 ? Version::kHttp10 : Version::kHttp11;
                thisPtr->httpVersion_ = version;
                thisPtr->transport_ =
                    std::make_unique<Http1xTransport>(connPtr,
                                                      version,
                                                      &thisPtr->bytesSent_,
                                                      &thisPtr->bytesReceived_);
            }
            else
            {
                LOG_ERROR << "Unknown protocol " << protocol
                          << " selected by server for HTTP";
                thisPtr->onError(ReqResult::BadResponse);
                return;
            }

            assert(thisPtr->httpVersion_.has_value());
            assert(thisPtr->transport_);
            thisPtr->transport_->setRespCallback(
                [weakPtr](const HttpResponseImplPtr &resp,
                          std::pair<HttpRequestPtr, HttpReqCallback> &&reqAndCb,
                          const trantor::TcpConnectionPtr &connPtr) {
                    auto thisPtr = weakPtr.lock();
                    if (!thisPtr)
                        return;
                    thisPtr->handleResponse(resp, std::move(reqAndCb), connPtr);
                });
            thisPtr->transport_->setErrorCallback([weakPtr](ReqResult result) {
                auto thisPtr = weakPtr.lock();
                if (!thisPtr)
                    return;
                thisPtr->onError(result);
            });

            size_t maxSendReq = (*(thisPtr->httpVersion_) == Version::kHttp2)
                                    ? size_t{0xfffffff}
                                    : thisPtr->pipeliningDepth_;
            if (maxSendReq == 0)
                maxSendReq = 1;
            while (!thisPtr->requestsBuffer_.empty() &&
                   thisPtr->transport_->requestsInFlight() < maxSendReq)
            {
                auto &reqAndCb = thisPtr->requestsBuffer_.front();
                thisPtr->transport_->sendRequestInLoop(reqAndCb.first,
                                                       std::move(
                                                           reqAndCb.second));
                thisPtr->requestsBuffer_.pop_front();
            }
        }
        else
        {
            LOG_TRACE << "connection disconnect";
            // TODO: Make sure the sequence of handling is correct
            bool isUnexpected = false;
            if (thisPtr->transport_)
            {
                isUnexpected = thisPtr->transport_->handleConnectionClose();
            }
            if (isUnexpected)
            {
                thisPtr->onError(ReqResult::NetworkFailure);
                return;
            }

            // temporary fix of dead tcpClientPtr_
            // TODO: fix HttpResponseParser when content-length absence
            // TODO: HTTP/2 transport also relies on this behavior to reconnect
            //   when running out of stream IDs
            thisPtr->tcpClientPtr_.reset();
            if (!thisPtr->requestsBuffer_.empty())
            {
                thisPtr->createTcpClient();
            }
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
                               std::optional<Version> targetVersion)
    : loop_(loop),
      serverAddr_(addr),
      useSSL_(useSSL),
      validateCert_(validateCert),
      useOldTLS_(useOldTLS),
      targetHttpVersion_(targetVersion)
{
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const std::string &hostString,
                               bool useOldTLS,
                               bool validateCert,
                               std::optional<Version> targetVersion)
    : loop_(loop),
      validateCert_(validateCert),
      useOldTLS_(useOldTLS),
      targetHttpVersion_(targetVersion)
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

struct RequestCallbackParams
{
    RequestCallbackParams(HttpReqCallback &&cb,
                          HttpClientImplPtr client,
                          HttpRequestPtr req)
        : callback(std::move(cb)),
          clientPtr(std::move(client)),
          requestPtr(std::move(req))
    {
    }

    const drogon::HttpReqCallback callback;
    const HttpClientImplPtr clientPtr;
    const HttpRequestPtr requestPtr;
    bool timeoutFlag{false};
};

void HttpClientImpl::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback,
                                       double timeout)
{
    if (timeout <= 0)
    {
        sendRequestInLoop(req, std::move(callback));
        return;
    }

    auto callbackParamsPtr =
        std::make_shared<RequestCallbackParams>(std::move(callback),
                                                shared_from_this(),
                                                req);

    loop_->runAfter(
        timeout,
        [weakCallbackBackPtr =
             std::weak_ptr<RequestCallbackParams>(callbackParamsPtr)] {
            auto callbackParamsPtr = weakCallbackBackPtr.lock();
            if (callbackParamsPtr != nullptr)
            {
                auto &thisPtr = callbackParamsPtr->clientPtr;
                if (callbackParamsPtr->timeoutFlag)
                {
                    return;
                }

                callbackParamsPtr->timeoutFlag = true;

                for (auto iter = thisPtr->requestsBuffer_.begin();
                     iter != thisPtr->requestsBuffer_.end();
                     ++iter)
                {
                    if (iter->first == callbackParamsPtr->requestPtr)
                    {
                        thisPtr->requestsBuffer_.erase(iter);
                        break;
                    }
                }

                (callbackParamsPtr->callback)(ReqResult::Timeout, nullptr);
            }
        });
    sendRequestInLoop(req,
                      [callbackParamsPtr](ReqResult r,
                                          const HttpResponsePtr &resp) {
                          if (callbackParamsPtr->timeoutFlag)
                          {
                              return;
                          }
                          callbackParamsPtr->timeoutFlag = true;
                          (callbackParamsPtr->callback)(r, resp);
                      });
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
                                       drogon::HttpReqCallback &&callback)
{
    loop_->assertInLoopThread();
    if (!static_cast<drogon::HttpRequestImpl *>(req.get())->passThrough())
    {
        req->addHeader("connection", "Keep-Alive");
        if (!userAgent_.empty())
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
        auto callbackPtr =
            std::make_shared<drogon::HttpReqCallback>(std::move(callback));
        requestsBuffer_.push_back(
            {req,
             [thisPtr = shared_from_this(),
              callbackPtr =
                  std::move(callbackPtr)](ReqResult result,
                                          const HttpResponsePtr &response) {
                 (*callbackPtr)(result, response);
             }});
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
                callback(ReqResult::BadServerAddress, nullptr);
                assert(requestsBuffer_.empty());
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
                    while (!(thisPtr->requestsBuffer_).empty())
                    {
                        auto &reqAndCb = (thisPtr->requestsBuffer_).front();
                        reqAndCb.second(ReqResult::BadServerAddress, nullptr);
                        (thisPtr->requestsBuffer_).pop_front();
                    }
                });
            });

        return;
    }

    // send request;
    auto connPtr = tcpClientPtr_->connection();
    auto thisPtr = shared_from_this();

    // Not connected, push request to buffer and wait for connection
    if (!connPtr || connPtr->disconnected())
    {
        requestsBuffer_.push_back(
            {req,
             [thisPtr,
              callback = std::move(callback)](ReqResult result,
                                              const HttpResponsePtr &response) {
                 callback(result, response);
             }});
        return;
    }
    assert(transport_ != nullptr);
    assert(httpVersion_.has_value());

    size_t maxSendReq = (*httpVersion_ == Version::kHttp2) ? size_t{0xfffffffff}
                                                           : pipeliningDepth_;
    if (maxSendReq == 0)
        maxSendReq = 1;

    // Connected, send request now
    if (transport_->requestsInFlight() <= maxSendReq && requestsBuffer_.empty())
    {
        transport_->sendRequestInLoop(req, std::move(callback));
    }
    else
    {
        requestsBuffer_.push_back(
            {req,
             [thisPtr,
              callback = std::move(callback)](ReqResult result,
                                              const HttpResponsePtr &response) {
                 callback(result, response);
             }});
    }
}

void HttpClientImpl::handleResponse(
    const HttpResponseImplPtr &resp,
    std::pair<HttpRequestPtr, HttpReqCallback> &&reqAndCb,
    const trantor::TcpConnectionPtr &connPtr)
{
    auto &type = resp->getHeaderBy("content-type");
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
    if (type.find("application/json") != std::string::npos)
    {
        resp->parseJson();
    }
    resp->setPeerCertificate(connPtr->peerCertificate());
    auto cb = std::move(reqAndCb);
    handleCookies(resp);
    cb.second(ReqResult::Ok, resp);

    // LOG_TRACE << "pipelining buffer size=" <<
    // pipeliningCallbacks_.size(); LOG_TRACE << "requests buffer size="
    // << requestsBuffer_.size();

    if (connPtr->connected())
    {
        if (!requestsBuffer_.empty())
        {
            auto &reqAndCallback = requestsBuffer_.front();
            transport_->sendRequestInLoop(reqAndCallback.first,
                                          std::move(reqAndCallback.second));
            requestsBuffer_.pop_front();
        }
        else
        {
            if (resp->ifCloseConnection() &&
                transport_->requestsInFlight() == 0)
            {
                tcpClientPtr_.reset();
            }
        }
    }
    else
    {
        transport_->onError(ReqResult::NetworkFailure);
    }
}

void HttpClientImpl::onRecvMessage(const trantor::TcpConnectionPtr &connPtr,
                                   trantor::MsgBuffer *msg)
{
    assert(transport_ != nullptr);
    transport_->onRecvMessage(connPtr, msg);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &ip,
                                        uint16_t port,
                                        bool useSSL,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert,
                                        std::optional<Version> targetVersion)
{
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL,
        useOldTLS,
        validateCert,
        targetVersion);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &hostString,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert,
                                        std::optional<Version> targetVersion)
{
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        useOldTLS,
        validateCert,
        targetVersion);
}

void HttpClientImpl::onError(ReqResult result)
{
    if (transport_)
        transport_->onError(result);
    while (!requestsBuffer_.empty())
    {
        auto cb = std::move(requestsBuffer_.front().second);
        requestsBuffer_.pop_front();
        cb(result, nullptr);
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
