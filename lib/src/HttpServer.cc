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

#include <trantor/utils/Logger.h>
#include "HttpServerContext.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <functional>

using namespace std::placeholders;
using namespace drogon;
using namespace trantor;

static void defaultHttpAsyncCallback(const HttpRequestPtr &, const std::function<void(const HttpResponsePtr &resp)> &callback)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultWebSockAsyncCallback(const HttpRequestPtr &,
                                        const std::function<void(const HttpResponsePtr &resp)> &callback,
                                        const WebSocketConnectionPtr &wsConnPtr)
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
    : _server(loop, listenAddr, name.c_str()),
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
    LOG_WARN << "HttpServer[" << _server.name()
             << "] starts listenning on " << _server.ipPort();
    _server.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpServerContext(conn));
    }
    else if (conn->disconnected())
    {
        LOG_TRACE << "conn disconnected!";
        HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());

        // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
        if (context->webSocketConn())
        {
            _disconnectWebsocketCallback(context->webSocketConn());
        }
        conn->setContext(std::string("None"));
    }
    _connectionCallback(conn);
}

void HttpServer::onMessage(const TcpConnectionPtr &conn,
                           MsgBuffer *buf)
{
    HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());

    // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
    if (context->webSocketConn())
    {
        //websocket payload,we shouldn't parse it
        _webSocketMessageCallback(context->webSocketConn(), buf);
        return;
    }
    if (!context->parseRequest(buf))
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        //conn->shutdown();
    }

    if (context->gotAll())
    {
        context->requestImpl()->parsePremeter();
        context->requestImpl()->setPeerAddr(conn->peerAddr());
        context->requestImpl()->setLocalAddr(conn->localAddr());
        context->requestImpl()->setReceiveDate(trantor::Date::date());
        if (context->firstReq() && isWebSocket(conn, context->request()))
        {
            auto wsConn = std::make_shared<WebSocketConnectionImpl>(conn);
            _newWebsocketCallback(context->request(),
                                  [=](const HttpResponsePtr &resp) mutable {
                                      if (resp->statusCode() == HttpResponse::k101SwitchingProtocols)
                                      {
                                          context->setWebsockConnection(wsConn);
                                      }
                                      auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                      conn->send(httpString);
                                  },
                                  wsConn);
        }
        else
            onRequest(conn, context->request());
        context->reset();
    }
}
bool HttpServer::isWebSocket(const TcpConnectionPtr &conn, const HttpRequestPtr &req)
{
    if (req->getHeader("Connection") == "Upgrade" &&
        req->getHeader("Upgrade") == "websocket")
    {
        LOG_TRACE << "new websocket request";

        return true;
    }
    return false;
}
void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequestPtr &req)
{
    const std::string &connection = req->getHeader("Connection");
    bool _close = connection == "close" ||
                  (req->getVersion() == HttpRequestImpl::kHttp10 && connection != "Keep-Alive");

    bool _isHeadMethod = (req->method() == Head);
    if (_isHeadMethod)
    {
        req->setMethod(Get);
    }
    HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());
    {
        //std::lock_guard<std::mutex> guard(context->getPipeLineMutex());
        context->pushRquestToPipeLine(req);
    }
    _httpAsyncCallback(req, [=](const HttpResponsePtr &response) {
        if (!response)
            return;
        response->setCloseConnection(_close);
        //if the request method is HEAD,remove the body of response(rfc2616-9.4)
        auto newResp = response;
        if (_isHeadMethod)
        {
            if (newResp->expiredTime() >= 0)
            {
                //Cached response,make a copy
                newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(response));
                newResp->setExpiredTime(-1);
            }
            newResp->setBody(std::string());
        }

        auto &sendfileName = std::dynamic_pointer_cast<HttpResponseImpl>(newResp)->sendfileName();

        if (HttpAppFramework::instance().useGzip() &&
            sendfileName.empty() &&
            req->getHeader("Accept-Encoding").find("gzip") != std::string::npos &&
            response->getHeader("Content-Encoding") == "" &&
            response->getContentTypeCode() < CT_APPLICATION_OCTET_STREAM &&
            response->getBody().length() > 1024)
        {
            //use gzip
            LOG_TRACE << "Use gzip to compress the body";
            char *zbuf = new char[response->getBody().length()];
            size_t zlen = response->getBody().length();
            if (gzipCompress(response->getBody().data(),
                             response->getBody().length(),
                             zbuf, &zlen) >= 0)
            {
                if (zlen > 0)
                {
                    if (response->expiredTime() >= 0)
                    {
                        //cached response,we need to make a clone
                        newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(response));
                        newResp->setExpiredTime(-1);
                    }
                    newResp->setBody(std::string(zbuf, zlen));
                    newResp->addHeader("Content-Encoding", "gzip");
                }
                else
                {
                    LOG_ERROR << "gzip got 0 length result";
                }
            }
            delete[] zbuf;
        }
        if (conn->getLoop()->isInLoopThread())
        {
            /*
             * A client that supports persistent connections MAY “pipeline”
             * its requests (i.e., send multiple requests without waiting
             * for each response). A server MUST send its responses to those
             * requests in the same order that the requests were received.
             *                                             rfc2616-8.1.1.2
             */
            //std::lock_guard<std::mutex> guard(context->getPipeLineMutex());
            if (context->getFirstRequest() == req)
            {
                context->popFirstRequest();
                sendResponse(conn, newResp);
                while (1)
                {
                    auto resp = context->getFirstResponse();
                    if (resp)
                    {
                        context->popFirstRequest();
                        sendResponse(conn, resp);
                    }
                    else
                        return;
                }
            }
            else
            {
                //some earlier requests are waiting for responses;
                context->pushResponseToPipeLine(req, newResp);
            }
        }
        else
        {
            conn->getLoop()->queueInLoop([conn, req, newResp, this]() {
                HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());
                if (context->getFirstRequest() == req)
                {
                    context->popFirstRequest();
                    sendResponse(conn, newResp);
                    while (1)
                    {
                        auto resp = context->getFirstResponse();
                        if (resp)
                        {
                            context->popFirstRequest();
                            sendResponse(conn, resp);
                        }
                        else
                            return;
                    }
                }
                else
                {
                    //some earlier requests are waiting for responses;
                    context->pushResponseToPipeLine(req, newResp);
                }
            });
        }
    });
}
void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponsePtr &response)
{
    auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(response)->renderToString();
    conn->send(httpString);
    auto &sendfileName = std::dynamic_pointer_cast<HttpResponseImpl>(response)->sendfileName();
    if (!sendfileName.empty())
    {
        conn->sendFile(sendfileName.c_str());
    }
    if (response->closeConnection())
    {
        conn->shutdown();
    }
}
