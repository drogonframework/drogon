/**
 *
 *  HttpServer.cc
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

#include "HttpServer.h"

#include "HttpAppFrameworkImpl.h"
#include "HttpRequestParser.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <functional>
#include <trantor/utils/Logger.h>

using namespace std::placeholders;
using namespace drogon;
using namespace trantor;

static bool isWebSocket(const HttpRequestImplPtr &req)
{
    auto &headers = req->headers();
    if (headers.find("upgrade") == headers.end() ||
        headers.find("connection") == headers.end())
        return false;
    if (req->getHeaderBy("connection").find("Upgrade") != std::string::npos &&
        req->getHeaderBy("upgrade") == "websocket")
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
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultConnectionCallback(const trantor::TcpConnectionPtr &conn)
{
    return;
}

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name)
#ifdef __linux__
    : _server(loop, listenAddr, name.c_str()),
#else
    : _server(loop, listenAddr, name.c_str(), true, false),
#endif
      _httpAsyncCallback(defaultHttpAsyncCallback),
      _newWebsocketCallback(defaultWebSockAsyncCallback),
      _connectionCallback(defaultConnectionCallback)
{
    _server.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, _1));
    _server.setRecvMessageCallback(
        std::bind(&HttpServer::onMessage, this, _1, _2));
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
    LOG_TRACE << "HttpServer[" << _server.name() << "] starts listenning on "
              << _server.ipPort();
    _server.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpRequestParser(conn));
        _connectionCallback(conn);
    }
    else if (conn->disconnected())
    {
        LOG_TRACE << "conn disconnected!";
        _connectionCallback(conn);
        HttpRequestParser *requestParser =
            any_cast<HttpRequestParser>(conn->getMutableContext());
        if (requestParser)
        {
            if (requestParser->webSocketConn())
            {
                requestParser->webSocketConn()->onClose();
            }
#if (CXX_STD > 14)
            conn->getMutableContext()->reset();  // reset(): since c++17
#else
            conn->getMutableContext()->clear();
#endif
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn, MsgBuffer *buf)
{
    HttpRequestParser *requestParser =
        any_cast<HttpRequestParser>(conn->getMutableContext());
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
        std::vector<HttpRequestImplPtr> requests;
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
                return;
            }
            if (requestParser->gotAll())
            {
                requestParser->requestImpl()->setPeerAddr(conn->peerAddr());
                requestParser->requestImpl()->setLocalAddr(conn->localAddr());
                requestParser->requestImpl()->setCreationDate(
                    trantor::Date::date());
                if (requestParser->firstReq() &&
                    isWebSocket(requestParser->requestImpl()))
                {
                    auto wsConn =
                        std::make_shared<WebSocketConnectionImpl>(conn);
                    _newWebsocketCallback(
                        requestParser->requestImpl(),
                        [=](const HttpResponsePtr &resp) mutable {
                            if (resp->statusCode() == k101SwitchingProtocols)
                            {
                                requestParser->setWebsockConnection(wsConn);
                            }
                            auto httpString =
                                std::dynamic_pointer_cast<HttpResponseImpl>(
                                    resp)
                                    ->renderToString();
                            conn->send(httpString);
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
        onRequests(conn, requests);
    }
}

void HttpServer::onRequests(const TcpConnectionPtr &conn,
                            const std::vector<HttpRequestImplPtr> &requests)
{
    if (requests.empty())
        return;
    auto responsePtrs =
        std::make_shared<std::vector<std::pair<HttpResponsePtr, bool>>>();
    auto loopFlagPtr = std::make_shared<bool>(true);

    HttpRequestParser *requestParser =
        any_cast<HttpRequestParser>(conn->getMutableContext());
    if (!requestParser)
        return;
    for (auto &req : requests)
    {
        if (!conn->connected())
        {
            return;
        }
        if (requestParser->isStop())
        {
            break;
        }
        const std::string &connection = req->getHeaderBy("connection");
        bool _close = connection == "close" ||
                      (req->getVersion() == HttpRequestImpl::kHttp10 &&
                       connection != "Keep-Alive");

        bool isHeadMethod = (req->method() == Head);
        if (isHeadMethod)
        {
            req->setMethod(Get);
        }
        requestParser->pushRquestToPipelining(req);
        if (HttpAppFrameworkImpl::instance().keepaliveRequestsNumber() > 0 &&
            requestParser->numberOfRequestsParsed() >=
                HttpAppFrameworkImpl::instance().keepaliveRequestsNumber())
        {
            requestParser->stop();
        }

        if (HttpAppFrameworkImpl::instance().pipeliningRequestsNumber() > 0 &&
            requestParser->numberOfRequestsInPipelining() >=
                HttpAppFrameworkImpl::instance().pipeliningRequestsNumber())
        {
            requestParser->stop();
        }

        _httpAsyncCallback(req, [=](const HttpResponsePtr &response) {
            if (!response)
                return;
            response->setCloseConnection(_close);

            auto newResp = response;
            auto &sendfileName =
                std::dynamic_pointer_cast<HttpResponseImpl>(newResp)
                    ->sendfileName();

            if (app().isGzipEnabled() && sendfileName.empty() &&
                req->getHeaderBy("accept-encoding").find("gzip") !=
                    std::string::npos &&
                std::dynamic_pointer_cast<HttpResponseImpl>(response)
                    ->getHeaderBy("content-encoding")
                    .empty() &&
                response->getContentType() < CT_APPLICATION_OCTET_STREAM &&
                response->getBody().length() > 1024)
            {
                // use gzip
                LOG_TRACE << "Use gzip to compress the body";
                size_t zlen = response->getBody().length();
                auto strCompress =
                    utils::gzipCompress(response->getBody().data(),
                                        response->getBody().length());
                if (strCompress)
                {
                    if (zlen > 0)
                    {
                        LOG_TRACE << "length after compressing:" << zlen;
                        if (response->expiredTime() >= 0)
                        {
                            // cached response,we need to make a clone
                            newResp = std::make_shared<HttpResponseImpl>(
                                *std::dynamic_pointer_cast<HttpResponseImpl>(
                                    response));
                            newResp->setExpiredTime(-1);
                        }
                        newResp->setBody(std::move(*strCompress));
                        newResp->addHeader("Content-Encoding", "gzip");
                    }
                    else
                    {
                        LOG_ERROR << "gzip got 0 length result";
                    }
                }
            }
            if (conn->getLoop()->isInLoopThread())
            {
                /*
                 * A client that supports persistent connections MAY
                 * “pipeline” its requests (i.e., send multiple requests
                 * without waiting for each response). A server MUST send
                 * its responses to those requests in the same order that
                 * the requests were received. rfc2616-8.1.1.2
                 */
                if (!conn->connected())
                    return;
                if (requestParser->getFirstRequest() == req)
                {
                    requestParser->popFirstRequest();
                    if (*loopFlagPtr)
                    {
                        (*responsePtrs).emplace_back(newResp, isHeadMethod);
                    }
                    else
                    {
                        std::vector<std::pair<HttpResponsePtr, bool>> resps;
                        resps.emplace_back(newResp, isHeadMethod);
                        while (requestParser->numberOfRequestsInPipelining() >
                               0)
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
                        sendResponses(conn, resps);
                    }
                    if (requestParser->isStop() &&
                        requestParser->numberOfRequestsInPipelining() == 0)
                    {
                        if (*loopFlagPtr)
                        {
                            sendResponses(conn, *responsePtrs);
                            responsePtrs->clear();
                        }
                        conn->shutdown();
                    }
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
                                              isHeadMethod]() {
                    HttpRequestParser *requestParser =
                        any_cast<HttpRequestParser>(conn->getMutableContext());
                    if (requestParser)
                    {
                        if (requestParser->getFirstRequest() == req)
                        {
                            requestParser->popFirstRequest();
                            std::vector<std::pair<HttpResponsePtr, bool>> resps;
                            resps.emplace_back(newResp, isHeadMethod);
                            while (
                                requestParser->numberOfRequestsInPipelining() >
                                0)
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
                            sendResponses(conn, resps);
                            if (requestParser->isStop() &&
                                requestParser->numberOfRequestsInPipelining() ==
                                    0)
                            {
                                conn->shutdown();
                            }
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
    }
    *loopFlagPtr = false;
    if (conn->connected() && !responsePtrs->empty())
        sendResponses(conn, *responsePtrs);
}

void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponsePtr &response,
                              bool isHeadMethod)
{
    conn->getLoop()->assertInLoopThread();
    auto respImplPtr = std::dynamic_pointer_cast<HttpResponseImpl>(response);
    if (!isHeadMethod)
    {
        auto httpString = respImplPtr->renderToString();
        conn->send(httpString);
        auto &sendfileName = respImplPtr->sendfileName();
        if (!sendfileName.empty())
        {
            conn->sendFile(sendfileName.c_str());
        }
    }
    else
    {
        auto httpString = respImplPtr->renderHeaderForHeadMethod();
        conn->send(httpString);
    }

    if (response->ifCloseConnection())
    {
        conn->shutdown();
    }
}

void HttpServer::sendResponses(
    const TcpConnectionPtr &conn,
    const std::vector<std::pair<HttpResponsePtr, bool>> &responses)
{
    conn->getLoop()->assertInLoopThread();
    if (responses.empty())
        return;
    if (responses.size() == 1)
    {
        sendResponse(conn, responses[0].first, responses[0].second);
        return;
    }
    trantor::MsgBuffer buffer(256 * responses.size());
    for (auto const &resp : responses)
    {
        auto respImplPtr =
            std::dynamic_pointer_cast<HttpResponseImpl>(resp.first);
        if (!resp.second)
        {
            // Not HEAD method
            auto httpString = respImplPtr->renderToString();
            auto &sendfileName = respImplPtr->sendfileName();
            if (!sendfileName.empty())
            {
                conn->send(buffer);
                buffer.retrieveAll();
                conn->send(httpString);
                conn->sendFile(sendfileName.c_str());
            }
            else
            {
                buffer.append(httpString->data(), httpString->length());
            }
        }
        else
        {
            auto httpString = respImplPtr->renderHeaderForHeadMethod();
            buffer.append(httpString->data(), httpString->length());
        }
        if (respImplPtr->ifCloseConnection())
        {
            if (buffer.readableBytes() > 0)
                conn->send(buffer);
            conn->shutdown();
            return;
        }
    }
    if (conn->connected() && buffer.readableBytes() > 0)
    {
        conn->send(buffer);
    }
}
