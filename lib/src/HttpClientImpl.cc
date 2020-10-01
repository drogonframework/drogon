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
#include "HttpResponseImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseParser.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/config.h>
#include <algorithm>
#include <stdlib.h>

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

#ifdef OpenSSL_FOUND
    if (useSSL_)
    {
        tcpClientPtr_->enableSSL(useOldTLS_);
    }
#endif
    auto thisPtr = shared_from_this();
    std::weak_ptr<HttpClientImpl> weakPtr = thisPtr;

    tcpClientPtr_->setConnectionCallback(
        [weakPtr](const trantor::TcpConnectionPtr &connPtr) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            if (connPtr->connected())
            {
                connPtr->setContext(std::make_shared<HttpResponseParser>());
                // send request;
                LOG_TRACE << "Connection established!";
                while (thisPtr->pipeliningCallbacks_.size() <=
                           thisPtr->pipeliningDepth_ &&
                       !thisPtr->requestsBuffer_.empty())
                {
                    thisPtr->sendReq(connPtr,
                                     thisPtr->requestsBuffer_.front().first);
                    thisPtr->pipeliningCallbacks_.push(
                        std::move(thisPtr->requestsBuffer_.front()));
                    thisPtr->requestsBuffer_.pop();
                }
            }
            else
            {
                LOG_TRACE << "connection disconnect";
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
    tcpClientPtr_->connect();
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const trantor::InetAddress &addr,
                               bool useSSL,
                               bool useOldTLS)
    : loop_(loop), serverAddr_(addr), useSSL_(useSSL), useOldTLS_(useOldTLS)
{
}

HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const std::string &hostString,
                               bool useOldTLS)
    : loop_(loop), useOldTLS_(useOldTLS)
{
    auto lowerHost = hostString;
    std::transform(lowerHost.begin(),
                   lowerHost.end(),
                   lowerHost.begin(),
                   tolower);
    if (lowerHost.find("https://") != std::string::npos)
    {
        useSSL_ = true;
        lowerHost = lowerHost.substr(8);
    }
    else if (lowerHost.find("http://") != std::string::npos)
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
    LOG_TRACE << "userSSL=" << useSSL_ << " domain=" << domain_;
}

HttpClientImpl::~HttpClientImpl()
{
    LOG_TRACE << "Deconstruction HttpClient";
    if (resolverPtr_ && !(loop_->isInLoopThread()))
    {
        // Make sure the resolverPtr_ is destroyed in the correct thread.
        loop_->queueInLoop([reolverPtr = resolverPtr_]() {});
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
void HttpClientImpl::sendRequestInLoop(const HttpRequestPtr &req,
                                       HttpReqCallback &&callback,
                                       double timeout)
{
    if (timeout <= 0)
    {
        sendRequestInLoop(req, std::move(callback));
    }
    else
    {
        auto timeoutFlag = std::make_shared<bool>(false);
        auto callbackPtr =
            std::make_shared<drogon::HttpReqCallback>(std::move(callback));
        loop_->runAfter(timeout, [timeoutFlag, callbackPtr] {
            if (*timeoutFlag)
            {
                return;
            }
            *timeoutFlag = true;
            (*callbackPtr)(ReqResult::Timeout, nullptr);
        });
        sendRequestInLoop(req,
                          [timeoutFlag,
                           callbackPtr](ReqResult r,
                                        const HttpResponsePtr &resp) {
                              if (*timeoutFlag)
                              {
                                  return;
                              }
                              *timeoutFlag = true;
                              (*callbackPtr)(r, resp);
                          });
    }
}
void HttpClientImpl::sendRequestInLoop(const drogon::HttpRequestPtr &req,
                                       drogon::HttpReqCallback &&callback)
{
    loop_->assertInLoopThread();
    if (!static_cast<drogon::HttpRequestImpl *>(req.get())->passThrough())
    {
        req->addHeader("connection", "Keep-Alive");
        req->addHeader("user-agent", "DrogonClient");
    }
    // Set the host header.
    if (!domain_.empty())
    {
        if ((useSSL_ && serverAddr_.toPort() != 443) ||
            (!useSSL_ && serverAddr_.toPort() != 80))
        {
            std::string host =
                domain_ + ":" + std::to_string(serverAddr_.toPort());
            req->addHeader("host", host);
        }
        else
        {
            req->addHeader("host", domain_);
        }
    }
    else
    {
        if ((useSSL_ && serverAddr_.toPort() != 443) ||
            (!useSSL_ && serverAddr_.toPort() != 80))
        {
            req->addHeader("host", serverAddr_.toIpPort());
        }
        else
        {
            req->addHeader("host", serverAddr_.toIp());
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
        requestsBuffer_.push(
            {req,
             [thisPtr = shared_from_this(),
              callback = std::move(callback)](ReqResult result,
                                              const HttpResponsePtr &response) {
                 callback(result, response);
             }});
        if (!dns_)
        {
            bool hasIpv6Address = false;
            if (serverAddr_.isIpV6())
            {
                auto ipaddr = serverAddr_.ip6NetEndian();
                for (int i = 0; i < 4; ++i)
                {
                    if (ipaddr[i] != 0)
                    {
                        hasIpv6Address = true;
                        break;
                    }
                }
            }

            if (serverAddr_.ipNetEndian() == 0 && !hasIpv6Address &&
                !domain_.empty() && serverAddr_.portNetEndian() != 0)
            {
                dns_ = true;
                if (!resolverPtr_)
                {
                    resolverPtr_ =
                        trantor::Resolver::newResolver(loop_,
                                                       kDefaultDNSTimeout);
                }
                resolverPtr_->resolve(
                    domain_,
                    [thisPtr = shared_from_this(),
                     hasIpv6Address](const trantor::InetAddress &addr) {
                        thisPtr->loop_->runInLoop([thisPtr,
                                                   addr,
                                                   hasIpv6Address]() {
                            auto port = thisPtr->serverAddr_.portNetEndian();
                            thisPtr->serverAddr_ = addr;
                            thisPtr->serverAddr_.setPortNetEndian(port);
                            LOG_TRACE << "dns:domain=" << thisPtr->domain_
                                      << ";ip=" << thisPtr->serverAddr_.toIp();
                            thisPtr->dns_ = false;
                            if ((thisPtr->serverAddr_.ipNetEndian() != 0 ||
                                 hasIpv6Address) &&
                                thisPtr->serverAddr_.portNetEndian() != 0)
                            {
                                thisPtr->createTcpClient();
                            }
                            else
                            {
                                while (!(thisPtr->requestsBuffer_).empty())
                                {
                                    auto &reqAndCb =
                                        (thisPtr->requestsBuffer_).front();
                                    reqAndCb.second(ReqResult::BadServerAddress,
                                                    nullptr);
                                    (thisPtr->requestsBuffer_).pop();
                                }
                                return;
                            }
                        });
                    });
                return;
            }

            if ((serverAddr_.ipNetEndian() != 0 || hasIpv6Address) &&
                serverAddr_.portNetEndian() != 0)
            {
                createTcpClient();
            }
            else
            {
                requestsBuffer_.pop();
                callback(ReqResult::BadServerAddress, nullptr);
                assert(requestsBuffer_.empty());
                return;
            }
        }
    }
    else
    {
        // send request;
        auto connPtr = tcpClientPtr_->connection();
        auto thisPtr = shared_from_this();
        if (connPtr && connPtr->connected())
        {
            if (pipeliningCallbacks_.size() <= pipeliningDepth_ &&
                requestsBuffer_.empty())
            {
                sendReq(connPtr, req);
                pipeliningCallbacks_.push(
                    {req,
                     [thisPtr, callback = std::move(callback)](
                         ReqResult result, const HttpResponsePtr &response) {
                         callback(result, response);
                     }});
            }
            else
            {
                requestsBuffer_.push(
                    {req,
                     [thisPtr, callback = std::move(callback)](
                         ReqResult result, const HttpResponsePtr &response) {
                         callback(result, response);
                     }});
            }
        }
        else
        {
            requestsBuffer_.push(
                {req,
                 [thisPtr, callback = std::move(callback)](
                     ReqResult result, const HttpResponsePtr &response) {
                     callback(result, response);
                 }});
        }
    }
}

void HttpClientImpl::sendReq(const trantor::TcpConnectionPtr &connPtr,
                             const HttpRequestPtr &req)
{
    trantor::MsgBuffer buffer;
    assert(req);
    auto implPtr = static_cast<HttpRequestImpl *>(req.get());
    implPtr->appendToBuffer(&buffer);
    LOG_TRACE << "Send request:"
              << std::string(buffer.peek(), buffer.readableBytes());
    bytesSent_ += buffer.readableBytes();
    connPtr->send(std::move(buffer));
}

void HttpClientImpl::onRecvMessage(const trantor::TcpConnectionPtr &connPtr,
                                   trantor::MsgBuffer *msg)
{
    auto responseParser = connPtr->getContext<HttpResponseParser>();

    // LOG_TRACE << "###:" << msg->readableBytes();
    auto msgSize = msg->readableBytes();
    while (msg->readableBytes() > 0)
    {
        assert(!pipeliningCallbacks_.empty());
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
        if (responseParser->gotAll())
        {
            auto resp = responseParser->responseImpl();
            responseParser->reset();
            assert(!pipeliningCallbacks_.empty());
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
            auto cb = std::move(firstReq);
            pipeliningCallbacks_.pop();
            handleCookies(resp);
            bytesReceived_ += (msgSize - msg->readableBytes());
            msgSize = msg->readableBytes();
            cb.second(ReqResult::Ok, resp);

            // LOG_TRACE << "pipelining buffer size=" <<
            // pipeliningCallbacks_.size(); LOG_TRACE << "requests buffer size="
            // << requestsBuffer_.size();

            if (!requestsBuffer_.empty())
            {
                auto &reqAndCb = requestsBuffer_.front();
                sendReq(connPtr, reqAndCb.first);
                pipeliningCallbacks_.push(std::move(reqAndCb));
                requestsBuffer_.pop();
            }
            else
            {
                if (resp->ifCloseConnection() && pipeliningCallbacks_.empty())
                {
                    tcpClientPtr_.reset();
                }
            }
        }
        else
        {
            break;
        }
    }
}

HttpClientPtr HttpClient::newHttpClient(const std::string &ip,
                                        uint16_t port,
                                        bool useSSL,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS)
{
    bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        trantor::InetAddress(ip, port, isIpv6),
        useSSL,
        useOldTLS);
}

HttpClientPtr HttpClient::newHttpClient(const std::string &hostString,
                                        trantor::EventLoop *loop,
                                        bool useOldTLS)
{
    return std::make_shared<HttpClientImpl>(
        loop == nullptr ? HttpAppFrameworkImpl::instance().getLoop() : loop,
        hostString,
        useOldTLS);
}

void HttpClientImpl::onError(ReqResult result)
{
    while (!pipeliningCallbacks_.empty())
    {
        auto cb = std::move(pipeliningCallbacks_.front());
        pipeliningCallbacks_.pop();
        cb.second(result, nullptr);
    }
    while (!requestsBuffer_.empty())
    {
        auto cb = std::move(requestsBuffer_.front().second);
        requestsBuffer_.pop();
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