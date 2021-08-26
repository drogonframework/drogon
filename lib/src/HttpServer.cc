/**
 *
 *  @file HttpServer.cc
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

#include "HttpServer.h"
#include "HttpRequestImpl.h"
#include "HttpRequestParser.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpResponseImpl.h"
#include "WebSocketConnectionImpl.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <functional>
#include <trantor/utils/Logger.h>

#if COZ_PROFILING
#include <coz.h>
#else
#define COZ_PROGRESS
#define COZ_PROGRESS_NAMED(name)
#define COZ_BEGIN(name)
#define COZ_END(name)
#endif

using namespace std::placeholders;
using namespace drogon;
using namespace trantor;
namespace drogon
{
static HttpResponsePtr getCompressedResponse(const HttpRequestImplPtr &req,
                                             const HttpResponsePtr &response,
                                             bool isHeadMethod)
{
    if (isHeadMethod ||
        !static_cast<HttpResponseImpl *>(response.get())->shouldBeCompressed())
    {
        return response;
    }
#ifdef USE_BROTLI
    if (app().isBrotliEnabled() &&
        req->getHeaderBy("accept-encoding").find("br") != std::string::npos)
    {
        auto newResp = response;
        auto strCompress = utils::brotliCompress(response->getBody().data(),
                                                 response->getBody().length());
        if (!strCompress.empty())
        {
            if (response->expiredTime() >= 0)
            {
                // cached response,we need to make a clone
                newResp = std::make_shared<HttpResponseImpl>(
                    *static_cast<HttpResponseImpl *>(response.get()));
                newResp->setExpiredTime(-1);
            }
            newResp->setBody(std::move(strCompress));
            newResp->addHeader("Content-Encoding", "br");
        }
        else
        {
            LOG_ERROR << "brotli got 0 length result";
        }
        return newResp;
    }
#endif
    if (app().isGzipEnabled() &&
        req->getHeaderBy("accept-encoding").find("gzip") != std::string::npos)
    {
        auto newResp = response;
        auto strCompress = utils::gzipCompress(response->getBody().data(),
                                               response->getBody().length());
        if (!strCompress.empty())
        {
            if (response->expiredTime() >= 0)
            {
                // cached response,we need to make a clone
                newResp = std::make_shared<HttpResponseImpl>(
                    *static_cast<HttpResponseImpl *>(response.get()));
                newResp->setExpiredTime(-1);
            }
            newResp->setBody(std::move(strCompress));
            newResp->addHeader("Content-Encoding", "gzip");
        }
        else
        {
            LOG_ERROR << "gzip got 0 length result";
        }
        return newResp;
    }
    return response;
}
static bool isWebSocket(const HttpRequestImplPtr &req)
{
    auto &headers = req->headers();
    if (headers.find("upgrade") == headers.end() ||
        headers.find("connection") == headers.end())
        return false;
    auto connectionField = req->getHeaderBy("connection");
    std::transform(connectionField.begin(),
                   connectionField.end(),
                   connectionField.begin(),
                   tolower);
    auto upgradeField = req->getHeaderBy("upgrade");
    std::transform(upgradeField.begin(),
                   upgradeField.end(),
                   upgradeField.begin(),
                   tolower);
    if (connectionField.find("upgrade") != std::string::npos &&
        upgradeField == "websocket")
    {
        LOG_TRACE << "new websocket request";

        return true;
    }
    return false;
}

static void defaultHttpAsyncCallback(
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &resp)> &&callback)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultWebSockAsyncCallback(
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &resp)> &&callback,
    const WebSocketConnectionImplPtr &)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultConnectionCallback(const trantor::TcpConnectionPtr &)
{
    return;
}
}  // namespace drogon
HttpServer::HttpServer(
    EventLoop *loop,
    const InetAddress &listenAddr,
    const std::string &name,
    const std::vector<std::function<HttpResponsePtr(const HttpRequestPtr &)>>
        &syncAdvices,
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &preSendingAdvices)
#ifdef __linux__
    : server_(loop, listenAddr, name.c_str()),
#else
    : server_(loop, listenAddr, name.c_str(), true, app().reusePort()),
#endif
      httpAsyncCallback_(defaultHttpAsyncCallback),
      newWebsocketCallback_(defaultWebSockAsyncCallback),
      connectionCallback_(defaultConnectionCallback),
      syncAdvices_(syncAdvices),
      preSendingAdvices_(preSendingAdvices)
{
    server_.setConnectionCallback(
        [this](const auto &conn) { this->onConnection(conn); });
    server_.setRecvMessageCallback(
        [this](const auto &conn, auto buff) { this->onMessage(conn, buff); });
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
    LOG_TRACE << "HttpServer[" << server_.name() << "] starts listenning on "
              << server_.ipPort();
    server_.start();
}
void HttpServer::stop()
{
    server_.stop();
}
void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        auto parser = std::make_shared<HttpRequestParser>(conn);
        parser->reset();
        conn->setContext(parser);
        connectionCallback_(conn);
    }
    else if (conn->disconnected())
    {
        LOG_TRACE << "conn disconnected!";
        connectionCallback_(conn);
        auto requestParser = conn->getContext<HttpRequestParser>();
        if (requestParser)
        {
            if (requestParser->webSocketConn())
            {
                requestParser->webSocketConn()->onClose();
            }
            conn->clearContext();
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn, MsgBuffer *buf)
{
    if (!conn->hasContext())
        return;
    auto requestParser = conn->getContext<HttpRequestParser>();
    if (!requestParser)
        return;
    // With the pipelining feature or web socket, it is possible to receice
    // multiple messages at once, so
    // the while loop is necessary
    if (requestParser->webSocketConn())
    {
        // Websocket payload
        requestParser->webSocketConn()->onNewMessage(conn, buf);
    }
    else
    {
        auto &requests = requestParser->getRequestBuffer();
        while (buf->readableBytes() > 0)
        {
            if (requestParser->isStop())
            {
                // The number of requests has reached the limit.
                buf->retrieveAll();
                return;
            }
            if (!requestParser->parseRequest(buf))
            {
                requestParser->reset();
                conn->forceClose();
                return;
            }
            if (requestParser->gotAll())
            {
                requestParser->requestImpl()->setPeerAddr(conn->peerAddr());
                requestParser->requestImpl()->setLocalAddr(conn->localAddr());
                requestParser->requestImpl()->setCreationDate(
                    trantor::Date::date());
                requestParser->requestImpl()->setSecure(
                    conn->isSSLConnection());
                if (requestParser->firstReq() &&
                    isWebSocket(requestParser->requestImpl()))
                {
                    auto wsConn =
                        std::make_shared<WebSocketConnectionImpl>(conn);
                    wsConn->setPingMessage("", std::chrono::seconds{30});
                    auto req = requestParser->requestImpl();
                    newWebsocketCallback_(
                        req,
                        [conn, wsConn, requestParser, this, req](
                            const HttpResponsePtr &resp) mutable {
                            if (conn->connected())
                            {
                                for (auto &advice : preSendingAdvices_)
                                {
                                    advice(req, resp);
                                }
                                if (resp->statusCode() ==
                                    k101SwitchingProtocols)
                                {
                                    requestParser->setWebsockConnection(wsConn);
                                }
                                auto httpString =
                                    ((HttpResponseImpl *)resp.get())
                                        ->renderToBuffer();
                                conn->send(httpString);
                                COZ_PROGRESS
                            }
                        },
                        wsConn);
                }
                else
                    requests.push_back(requestParser->requestImpl());
                requestParser->reset();
            }
            else
            {
                break;
            }
        }
        onRequests(conn, requests, requestParser);
        requests.clear();
    }
}

struct CallBackParamPack
{
    CallBackParamPack() = default;
    CallBackParamPack(const trantor::TcpConnectionPtr &conn_,
                      const HttpRequestImplPtr &req_,
                      const std::shared_ptr<bool> &loopFlag_,
                      const std::shared_ptr<HttpRequestParser> &requestParser_,
                      bool *syncFlagPtr_,
                      bool close_,
                      bool isHeadMethod_)
        : conn(conn_),
          req(req_),
          loopFlag(loopFlag_),
          requestParser(requestParser_),
          syncFlagPtr(syncFlagPtr_),
          close(close_),
          isHeadMethod(isHeadMethod_)
    {
    }
    trantor::TcpConnectionPtr conn;
    HttpRequestImplPtr req;
    std::shared_ptr<bool> loopFlag;
    std::shared_ptr<HttpRequestParser> requestParser;
    bool *syncFlagPtr;
    bool close;
    bool isHeadMethod;
};

void HttpServer::onRequests(
    const TcpConnectionPtr &conn,
    const std::vector<HttpRequestImplPtr> &requests,
    const std::shared_ptr<HttpRequestParser> &requestParser)
{
    if (requests.empty())
        return;
    if (HttpAppFrameworkImpl::instance().keepaliveRequestsNumber() > 0 &&
        requestParser->numberOfRequestsParsed() >=
            HttpAppFrameworkImpl::instance().keepaliveRequestsNumber())
    {
        requestParser->stop();
        conn->shutdown();
        return;
    }
    else if (HttpAppFrameworkImpl::instance().pipeliningRequestsNumber() > 0 &&
             requestParser->numberOfRequestsInPipelining() + requests.size() >=
                 HttpAppFrameworkImpl::instance().pipeliningRequestsNumber())
    {
        requestParser->stop();
        conn->shutdown();
        return;
    }
    if (!conn->connected())
    {
        return;
    }
    auto loopFlagPtr = std::make_shared<bool>(true);

    for (auto &req : requests)
    {
        bool close_ = (!req->keepAlive());
        bool isHeadMethod = (req->method() == Head);
        if (isHeadMethod)
        {
            req->setMethod(Get);
        }
        bool syncFlag = false;
        if (!requestParser->emptyPipelining())
        {
            requestParser->pushRequestToPipelining(req);
            syncFlag = true;
        }
        if (!syncAdvices_.empty())
        {
            bool adviceFlag = false;
            for (auto &advice : syncAdvices_)
            {
                auto resp = advice(req);
                if (resp)
                {
                    resp->setVersion(req->getVersion());
                    resp->setCloseConnection(close_);
                    if (!syncFlag)
                    {
                        requestParser->getResponseBuffer().emplace_back(
                            getCompressedResponse(req, resp, isHeadMethod),
                            isHeadMethod);
                    }
                    else
                    {
                        requestParser->pushResponseToPipelining(
                            req,
                            getCompressedResponse(req, resp, isHeadMethod),
                            isHeadMethod);
                    }

                    adviceFlag = true;
                    break;
                }
            }
            if (adviceFlag)
                continue;
        }

        // Optimization: Avoids dynamic allocation when copying the callback in
        // handlers (ex: copying callback into lambda captures in DB calls)
        auto paramPack = std::make_shared<CallBackParamPack>(conn,
                                                             req,
                                                             loopFlagPtr,
                                                             requestParser,
                                                             &syncFlag,
                                                             close_,
                                                             isHeadMethod);
        httpAsyncCallback_(
            req,
            [paramPack = std::move(paramPack),
             this](const HttpResponsePtr &response) {
                auto &conn = paramPack->conn;
                auto &close_ = paramPack->close;
                auto &req = paramPack->req;
                auto &syncFlag = *paramPack->syncFlagPtr;
                auto &isHeadMethod = paramPack->isHeadMethod;
                auto &loopFlagPtr = paramPack->loopFlag;
                auto &requestParser = paramPack->requestParser;

                if (!response)
                    return;
                if (!conn->connected())
                    return;

                response->setVersion(req->getVersion());
                response->setCloseConnection(close_);
                for (auto &advice : preSendingAdvices_)
                {
                    advice(req, response);
                }
                auto newResp =
                    getCompressedResponse(req, response, isHeadMethod);
                if (conn->getLoop()->isInLoopThread())
                {
                    /*
                     * A client that supports persistent connections MAY
                     * "pipeline" its requests (i.e., send multiple requests
                     * without waiting for each response). A server MUST send
                     * its responses to those requests in the same order that
                     * the requests were received. rfc2616-8.1.1.2
                     */
                    if (!conn->connected())
                        return;
                    if (*loopFlagPtr)
                    {
                        syncFlag = true;
                        if (requestParser->emptyPipelining())
                        {
                            requestParser->getResponseBuffer().emplace_back(
                                newResp, isHeadMethod);
                        }
                        else
                        {
                            // some earlier requests are waiting for responses;
                            requestParser->pushResponseToPipelining(
                                req, newResp, isHeadMethod);
                        }
                    }
                    else if (requestParser->getFirstRequest() == req)
                    {
                        requestParser->popFirstRequest();

                        std::vector<std::pair<HttpResponsePtr, bool>> resps;
                        resps.emplace_back(newResp, isHeadMethod);
                        while (!requestParser->emptyPipelining())
                        {
                            auto resp = requestParser->getFirstResponse();
                            if (resp.first)
                            {
                                requestParser->popFirstRequest();
                                resps.push_back(std::move(resp));
                            }
                            else
                                break;
                        }
                        sendResponses(conn, resps, requestParser->getBuffer());
                    }
                    else
                    {
                        // some earlier requests are waiting for responses;
                        requestParser->pushResponseToPipelining(req,
                                                                newResp,
                                                                isHeadMethod);
                    }
                }
                else
                {
                    conn->getLoop()->queueInLoop([conn,
                                                  req,
                                                  newResp,
                                                  this,
                                                  isHeadMethod,
                                                  requestParser]() {
                        if (conn->connected())
                        {
                            if (requestParser->getFirstRequest() == req)
                            {
                                requestParser->popFirstRequest();
                                std::vector<std::pair<HttpResponsePtr, bool>>
                                    resps;
                                resps.emplace_back(newResp, isHeadMethod);
                                while (!requestParser->emptyPipelining())
                                {
                                    auto resp =
                                        requestParser->getFirstResponse();
                                    if (resp.first)
                                    {
                                        requestParser->popFirstRequest();
                                        resps.push_back(std::move(resp));
                                    }
                                    else
                                        break;
                                }
                                sendResponses(conn,
                                              resps,
                                              requestParser->getBuffer());
                            }
                            else
                            {
                                // some earlier requests are waiting for
                                // responses;
                                requestParser->pushResponseToPipelining(
                                    req, newResp, isHeadMethod);
                            }
                        }
                    });
                }
            });
        if (syncFlag == false)
        {
            requestParser->pushRequestToPipelining(req);
        }
    }
    *loopFlagPtr = false;
    if (conn->connected() && !requestParser->getResponseBuffer().empty())
    {
        sendResponses(conn,
                      requestParser->getResponseBuffer(),
                      requestParser->getBuffer());
        requestParser->getResponseBuffer().clear();
    }
}

void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponsePtr &response,
                              bool isHeadMethod)
{
    conn->getLoop()->assertInLoopThread();
    auto respImplPtr = static_cast<HttpResponseImpl *>(response.get());
    if (!isHeadMethod)
    {
        auto httpString = respImplPtr->renderToBuffer();
        conn->send(httpString);
        const std::string &sendfileName = respImplPtr->sendfileName();
        if (!sendfileName.empty())
        {
            const auto &range = respImplPtr->sendfileRange();
            conn->sendFile(sendfileName.c_str(), range.first, range.second);
        }
        COZ_PROGRESS
    }
    else
    {
        auto httpString = respImplPtr->renderHeaderForHeadMethod();
        conn->send(std::move(*httpString));
        COZ_PROGRESS
    }

    if (response->ifCloseConnection())
    {
        conn->shutdown();
        COZ_PROGRESS
    }
}

void HttpServer::sendResponses(
    const TcpConnectionPtr &conn,
    const std::vector<std::pair<HttpResponsePtr, bool>> &responses,
    trantor::MsgBuffer &buffer)
{
    conn->getLoop()->assertInLoopThread();
    if (responses.empty())
        return;
    if (responses.size() == 1)
    {
        sendResponse(conn, responses[0].first, responses[0].second);
        return;
    }
    for (auto const &resp : responses)
    {
        auto respImplPtr = static_cast<HttpResponseImpl *>(resp.first.get());
        if (!resp.second)
        {
            // Not HEAD method
            respImplPtr->renderToBuffer(buffer);
            const std::string &sendfileName = respImplPtr->sendfileName();
            if (!sendfileName.empty())
            {
                const auto &range = respImplPtr->sendfileRange();
                conn->send(buffer);
                buffer.retrieveAll();
                conn->sendFile(sendfileName.c_str(), range.first, range.second);
                COZ_PROGRESS
            }
        }
        else
        {
            auto httpString = respImplPtr->renderHeaderForHeadMethod();
            buffer.append(httpString->peek(), httpString->readableBytes());
        }
        if (respImplPtr->ifCloseConnection())
        {
            if (buffer.readableBytes() > 0)
            {
                conn->send(buffer);
                buffer.retrieveAll();
                COZ_PROGRESS
            }
            conn->shutdown();
            return;
        }
    }
    if (conn->connected() && buffer.readableBytes() > 0)
    {
        conn->send(buffer);
        COZ_PROGRESS
    }
    buffer.retrieveAll();
}
