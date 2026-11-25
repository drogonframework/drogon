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
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace trantor;
using namespace drogon;
using namespace std::placeholders;

namespace trantor
{
static const size_t kDefaultDNSTimeout{600};
}

TLSPolicyPtr HttpClientImpl::makeTlsPolicy() const
{
    auto policy = TLSPolicy::defaultClientPolicy();
    policy->setUseOldTLS(useOldTLS_)
        .setValidate(validateCert_)
        .setHostname(domain_)
        .setConfCmds(sslConfCmds_)
        .setCertPath(clientCertPath_)
        .setKeyPath(clientKeyPath_);
    return policy;
}

void HttpClientImpl::sendBufferedRequests(const TcpConnectionPtr &connPtr)
{
    while (pipeliningCallbacks_.size() <= pipeliningDepth_ &&
           !requestsBuffer_.empty())
    {
        sendReq(connPtr, requestsBuffer_.front().first);
        pipeliningCallbacks_.push(std::move(requestsBuffer_.front()));
        popFrontRequest();
    }
}

void HttpClientImpl::createTcpClient(const InetAddress &address)
{
    tunnelReady_ = false;
    connectionClosing_ = false;
    LOG_TRACE << "New TcpClient," << address.toIpPort();
    tcpClientPtr_ = std::make_shared<TcpClient>(loop_, address, "httpClient");

    if (useSSL_ && !usesTunnel() && utils::supportsTls())
    {
        LOG_TRACE << "useOldTLS=" << useOldTLS_;
        LOG_TRACE << "domain=" << domain_;
        tcpClientPtr_->enableSSL(makeTlsPolicy());
    }

    auto thisPtr = shared_from_this();
    std::weak_ptr<HttpClientImpl> weakPtr = thisPtr;
    auto currentClient =
        [weakPtr, weakClient = std::weak_ptr<TcpClient>(tcpClientPtr_)]() {
            auto client = weakPtr.lock();
            if (client && client->tcpClientPtr_ &&
                weakClient.lock() == client->tcpClientPtr_)
                return client;
            return HttpClientImplPtr{};
        };
    tcpClientPtr_->setSockOptCallback([weakPtr](int fd) {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        if (thisPtr->sockOptCallback_)
            thisPtr->sockOptCallback_(fd);
    });
    tcpClientPtr_->setConnectionCallback(
        [currentClient](const TcpConnectionPtr &connPtr) {
            auto thisPtr = currentClient();
            if (!thisPtr)
                return;
            if (connPtr->connected())
            {
                auto parser = std::make_shared<HttpResponseParser>(connPtr);
                connPtr->setContext(parser);
                LOG_TRACE << "Connection established!";
                if (thisPtr->usesTunnel())
                {
                    parser->setMaxHeaderSize(64 * 1024);
                    const auto &target = thisPtr->proxy_->targetAuthority;
                    auto request = "CONNECT " + target +
                                   " HTTP/1.1\r\nHost: " + target + "\r\n\r\n";
                    thisPtr->bytesSent_ += request.size();
                    connPtr->send(std::move(request));
                }
                else
                {
                    thisPtr->sendBufferedRequests(connPtr);
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
                    if (firstReq.first->method() == Head)
                    {
                        responseParser->setForHeadMethod();
                    }
                    auto resp = responseParser->responseImpl();
                    responseParser->reset();
                    thisPtr->handleResponse(resp, std::move(firstReq), connPtr);
                    return;
                }
                thisPtr->onError(ReqResult::NetworkFailure);
            }
        });
    tcpClientPtr_->setConnectionErrorCallback([currentClient]() {
        if (auto thisPtr = currentClient())
            thisPtr->onError(ReqResult::BadServerAddress);
    });
    tcpClientPtr_->setMessageCallback(
        [currentClient](const TcpConnectionPtr &connPtr, MsgBuffer *msg) {
            if (auto thisPtr = currentClient())
                thisPtr->onRecvMessage(connPtr, msg);
        });
    tcpClientPtr_->setSSLErrorCallback([currentClient](SSLError err) {
        auto thisPtr = currentClient();
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

void HttpClientImpl::onProxyConnectResponse(const TcpConnectionPtr &connPtr,
                                            MsgBuffer *msg)
{
    auto parser = connPtr->getContext<HttpResponseParser>();
    while (msg->readableBytes() != 0)
    {
        const auto size = msg->readableBytes();
        const bool parsed = parser->parseResponse(msg, true);
        bytesReceived_ += size - msg->readableBytes();
        if (!parsed)
        {
            onError(ReqResult::BadResponse);
            return;
        }
        if (!parser->gotAll())
            return;

        const auto code =
            static_cast<int>(parser->responseImpl()->statusCode());
        parser->reset();
        if (code >= 100 && code < 200 && code != k101SwitchingProtocols)
            continue;
        if (code < 200 || code >= 300)
        {
            onError(ReqResult::BadResponse);
            return;
        }

        parser->setMaxHeaderSize(0);
        connPtr->startEncryption(
            makeTlsPolicy(),
            false,
            [weakPtr = weak_from_this()](const TcpConnectionPtr &connection) {
                auto thisPtr = weakPtr.lock();
                if (thisPtr && thisPtr->tcpClientPtr_ &&
                    thisPtr->tcpClientPtr_->connection() == connection &&
                    connection->connected())
                {
                    thisPtr->tunnelReady_ = true;
                    thisPtr->sendBufferedRequests(connection);
                }
            });
        return;
    }
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const trantor::InetAddress &addr,
                               bool useSSL,
                               bool useOldTLS,
                               bool validateCert)
    : loop_(loop),
      serverAddr_(addr),
      useSSL_(useSSL),
      validateCert_(validateCert),
      useOldTLS_(useOldTLS)
{
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const std::string &hostString,
                               bool useOldTLS,
                               bool validateCert)
    : loop_(loop), validateCert_(validateCert), useOldTLS_(useOldTLS)
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

namespace
{
struct HttpEndpoint
{
    std::string host;
    InetAddress address;
    bool secure;
};

HttpEndpoint parseHttpEndpoint(std::string url, bool allowHttps)
{
    std::transform(url.begin(), url.end(), url.begin(), [](unsigned char ch) {
        return tolower(ch);
    });
    const bool secure = allowHttps && url.compare(0, 8, "https://") == 0;
    if (!secure && url.compare(0, 7, "http://") != 0)
        throw std::invalid_argument("Invalid HTTP endpoint scheme");

    std::string_view authority(url);
    authority.remove_prefix(secure ? 8 : 7);
    if (!authority.empty() && authority.back() == '/')
        authority.remove_suffix(1);

    const bool ipv6 = !authority.empty() && authority.front() == '[';
    std::string_view host;
    if (ipv6)
    {
        const auto end = authority.find(']');
        if (end == std::string_view::npos)
            throw std::invalid_argument("Invalid IPv6 endpoint");
        host = authority.substr(1, end - 1);
    }
    else
    {
        host = authority.substr(0, authority.find(':'));
    }
    authority.remove_prefix(host.size() + (ipv6 ? 2 : 0));
    const auto *validHostCharacters =
        ipv6 ? "0123456789abcdef:." : "abcdefghijklmnopqrstuvwxyz0123456789-._";
    if (host.empty() ||
        host.find_first_not_of(validHostCharacters) != std::string_view::npos)
        throw std::invalid_argument("Invalid HTTP endpoint host");

    uint16_t port = secure ? 443 : 80;
    if (!authority.empty())
    {
        const auto end = authority.data() + authority.size();
        const auto parsed = std::from_chars(authority.data() + 1, end, port);
        if (authority.front() != ':' || parsed.ec != std::errc() ||
            parsed.ptr != end || port == 0)
            throw std::invalid_argument("Invalid HTTP endpoint port");
    }

    const std::string hostname(host);
    const InetAddress address(hostname, port, ipv6);
    if (ipv6 && address.isUnspecified())
        throw std::invalid_argument("Invalid IPv6 endpoint");
    return {hostname, address, secure};
}
}  // namespace

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const std::string &hostString,
                               const std::string &proxyUrl,
                               bool useOldTLS,
                               bool validateCert)
    : loop_(loop), validateCert_(validateCert), useOldTLS_(useOldTLS)
{
    auto destination = parseHttpEndpoint(hostString, true);
    auto proxy = parseHttpEndpoint(proxyUrl, false);
    serverAddr_ = destination.address;
    domain_ = std::move(destination.host);
    useSSL_ = destination.secure;

    auto authority = serverAddr_.isIpV6() ? "[" + domain_ + "]" : domain_;
    if (useSSL_ || !onDefaultPort())
        authority += ":" + std::to_string(port());
    proxy_.emplace(
        Proxy{std::move(proxy.host), proxy.address, std::move(authority)});
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

    drogon::HttpReqCallback callback;
    HttpClientImplPtr clientPtr;
    const HttpRequestPtr requestPtr;
    bool timeoutFlag{false};
};

void HttpClientImpl::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback,
                                       double timeout)
{
    if (timeout <= 0)
    {
        auto callbackWithClient =
            [thisPtr = shared_from_this(),
             callback = std::move(callback)](ReqResult result,
                                             const HttpResponsePtr &response) {
                callback(result, response);
            };
        sendRequestInLoop(req, std::move(callbackWithClient));
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
                if (callbackParamsPtr->timeoutFlag)
                {
                    return;
                }

                callbackParamsPtr->timeoutFlag = true;
                // Release ownership after completion, even if no response ever
                // arrives. Keep the client alive while invoking the callback.
                auto thisPtr = std::move(callbackParamsPtr->clientPtr);
                auto callback = std::exchange(callbackParamsPtr->callback, {});

                for (auto iter = thisPtr->requestsBuffer_.begin();
                     iter != thisPtr->requestsBuffer_.end();
                     ++iter)
                {
                    if (iter->first == callbackParamsPtr->requestPtr)
                    {
                        thisPtr->eraseRequest(iter);
                        thisPtr->outstandingRequests_.fetch_sub(
                            1, std::memory_order_relaxed);
                        if (thisPtr->usesTunnel() && !thisPtr->tunnelReady_ &&
                            thisPtr->requestsBuffer_.empty())
                            thisPtr->tcpClientPtr_.reset();
                        break;
                    }
                }

                // An already sent request must stay in the response queue so
                // a late response cannot be mistaken for the next request's.
                callback(ReqResult::Timeout, nullptr);
            }
        });
    sendRequestInLoop(
        req, [callbackParamsPtr](ReqResult r, const HttpResponsePtr &resp) {
            if (callbackParamsPtr->timeoutFlag)
            {
                return;
            }
            callbackParamsPtr->timeoutFlag = true;
            auto thisPtr = std::move(callbackParamsPtr->clientPtr);
            auto callback = std::exchange(callbackParamsPtr->callback, {});
            callback(r, resp);
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
    outstandingRequests_.fetch_add(1, std::memory_order_relaxed);

    loop_->assertInLoopThread();
    // The completion wrapper owns the client until completion or timeout.
    // Queue entries must not add another client reference that outlives it.
    for (const auto &header : headers_)
    {
        if (proxy_ && header.first == "proxy-authorization")
            continue;
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
        if (proxy_)
        {
            req->addHeader("host", proxy_->targetAuthority);
        }
        else if (onDefaultPort())
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
        enqueueRequest(req, std::move(callback));
        connectToEndpoint();
        return;
    }

    if (connectionClosing_ || (usesTunnel() && !tunnelReady_))
    {
        enqueueRequest(req, std::move(callback));
        return;
    }

    // send request;
    auto connPtr = tcpClientPtr_->connection();
    // Not connected, push request to buffer and wait for connection
    if (!connPtr || connPtr->disconnected())
    {
        enqueueRequest(req, callback);
        return;
    }

    // Connected, send request now
    if (pipeliningCallbacks_.size() <= pipeliningDepth_ &&
        requestsBuffer_.empty())
    {
        sendReq(connPtr, req);
        pipeliningCallbacks_.push({req, std::move(callback)});
    }
    else
    {
        enqueueRequest(req, std::move(callback));
    }
}

void HttpClientImpl::connectToEndpoint()
{
    if (tcpClientPtr_ || dns_ || requestsBuffer_.empty())
        return;

    if (usesTunnel() && !utils::supportsTls())
    {
        onError(ReqResult::HandshakeError);
        return;
    }

    const auto &connectHost = proxy_ ? proxy_->host : domain_;
    const auto &address = connectAddress();
    if (proxy_ ? !address.isUnspecified()
               : (connectHost.empty() || !isDomainName_))
    {
        if (isValidIpAddr(address))
            createTcpClient(address);
        else
            onError(ReqResult::BadServerAddress);
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
        connectHost, [thisPtr](const trantor::InetAddress &addr) {
            thisPtr->loop_->runInLoop([thisPtr, addr]() {
                auto address = addr;
                address.setPortNetEndian(
                    thisPtr->connectAddress().portNetEndian());
                if (!thisPtr->proxy_)
                    thisPtr->serverAddr_ = address;
                LOG_TRACE << "dns:ip=" << address.toIp();
                thisPtr->dns_ = false;

                if (thisPtr->usesTunnel() && thisPtr->requestsBuffer_.empty())
                    return;

                if (isValidIpAddr(address))
                {
                    thisPtr->createTcpClient(address);
                    return;
                }
                thisPtr->onError(ReqResult::BadServerAddress);
            });
        });
}

void HttpClientImpl::sendReq(const trantor::TcpConnectionPtr &connPtr,
                             const HttpRequestPtr &req)
{
    trantor::MsgBuffer buffer;
    assert(req);
    auto implPtr = static_cast<HttpRequestImpl *>(req.get());
    std::string_view proxyTarget;
    const std::string *proxyAuthorization = nullptr;
    if (proxy_ && !useSSL_)
    {
        proxyTarget = proxy_->targetAuthority;
        auto header = headers_.find("proxy-authorization");
        if (header != headers_.end())
            proxyAuthorization = &header->second;
    }
    implPtr->appendToBuffer(&buffer,
                            proxyTarget,
                            usesTunnel(),
                            proxyAuthorization);
    if (!proxyTarget.empty())
    {
        LOG_TRACE << "Send proxy request: " << buffer.readableBytes()
                  << " bytes";
    }
    else
    {
        LOG_TRACE << "Send request:"
                  << std::string(buffer.peek(), buffer.readableBytes());
    }
    bytesSent_ += buffer.readableBytes();
    connPtr->send(std::move(buffer));
}

bool HttpClientImpl::handleResponse(
    const HttpResponseImplPtr &resp,
    std::pair<HttpRequestPtr, HttpReqCallback> &&reqAndCb,
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
    auto cb = std::move(reqAndCb);
    pipeliningCallbacks_.pop();
    outstandingRequests_.fetch_sub(1, std::memory_order_relaxed);
    handleCookies(resp);

    if (connectionClosing_ || !connPtr->connected())
    {
        // Detach the closing connection before callbacks can submit more work.
        // Requests already sent on this connection must not be replayed.
        auto inFlight = std::exchange(pipeliningCallbacks_, {});
        outstandingRequests_.fetch_sub(inFlight.size(),
                                       std::memory_order_relaxed);
        tcpClientPtr_.reset();
        cb.second(ReqResult::Ok, resp);
        while (!inFlight.empty())
        {
            inFlight.front().second(ReqResult::NetworkFailure, nullptr);
            inFlight.pop();
        }
        connectToEndpoint();
        return false;
    }

    cb.second(ReqResult::Ok, resp);
    if (connPtr->connected() && !requestsBuffer_.empty())
    {
        auto &reqAndCallback = requestsBuffer_.front();
        sendReq(connPtr, reqAndCallback.first);
        pipeliningCallbacks_.push(std::move(reqAndCallback));
        popFrontRequest();
    }
    return true;
}

void HttpClientImpl::onRecvMessage(const trantor::TcpConnectionPtr &connPtr,
                                   trantor::MsgBuffer *msg)
{
    if (usesTunnel() && !tunnelReady_)
    {
        onProxyConnectResponse(connPtr, msg);
        return;
    }

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
        if (firstReq.first->method() == Head)
        {
            responseParser->setForHeadMethod();
        }
        if (!responseParser->parseResponse(msg))
        {
            onError(ReqResult::BadResponse);
            bytesReceived_ += (msgSize - msg->readableBytes());
            return;
        }
        if (responseParser->shouldCloseConnection())
            connectionClosing_ = true;
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
            if (!handleResponse(resp, std::move(firstReq), connPtr))
                return;
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
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL,
        useOldTLS,
        validateCert);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &hostString,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS,
                                        bool validateCert)
{
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        useOldTLS,
        validateCert);
}

HttpClientPtr HttpClient::newHttpClientWithProxy(const std::string &hostString,
                                                 const std::string &proxyUrl,
                                                 trantor::EventLoop *loop,
                                                 bool useOldTLS,
                                                 bool validateCert)
{
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        proxyUrl,
        useOldTLS,
        validateCert);
}

void HttpClientImpl::onError(ReqResult result)
{
    // Detach the failed connection and its requests before invoking callbacks.
    // A callback may submit another request and start a new connection.
    auto pipelined = std::exchange(pipeliningCallbacks_, {});
    auto buffered = std::exchange(requestsBuffer_, {});
    outstandingRequests_.fetch_sub(pipelined.size() + buffered.size(),
                                   std::memory_order_relaxed);
    requestsBufferSize_.fetch_sub(buffered.size(), std::memory_order_relaxed);
    tcpClientPtr_.reset();

    while (!pipelined.empty())
    {
        pipelined.front().second(result, nullptr);
        pipelined.pop();
    }
    for (auto &request : buffered)
        request.second(result, nullptr);
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
