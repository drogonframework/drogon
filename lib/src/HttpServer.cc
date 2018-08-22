// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

//taken from muduo and modified
/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include "HttpServer.h"

#include <trantor/utils/Logger.h>
#include "HttpContext.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

using namespace std::placeholders;
using namespace drogon;
using namespace trantor;


static void defaultHttpAsyncCallback(const HttpRequestPtr&,const std::function<void( HttpResponse& resp)> & callback)
{
    HttpResponseImpl resp;
    resp.setStatusCode(HttpResponse::k404NotFound);
    resp.setCloseConnection(true);
    callback(resp);
}

static void defaultWebSockAsyncCallback(const HttpRequestPtr&,
                                        const std::function<void( HttpResponse& resp)> & callback,
                                        const WebSocketConnectionPtr& wsConnPtr)
{
    HttpResponseImpl resp;
    resp.setStatusCode(HttpResponse::k404NotFound);
    resp.setCloseConnection(true);
    callback(resp);
}




HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const std::string& name)
        : server_(loop, listenAddr, name.c_str()),
          httpAsyncCallback_(defaultHttpAsyncCallback),
          newWebsocketCallback_(defaultWebSockAsyncCallback)
{
    server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, _1));
    server_.setRecvMessageCallback(
            std::bind(&HttpServer::onMessage, this, _1, _2));
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name()
             << "] starts listenning on " << server_.ipPort();
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        conn->setContext(HttpContext());
    }
    else if(conn->disconnected())
    {
        LOG_TRACE<<"conn disconnected!";
        HttpContext* context = any_cast<HttpContext>(conn->getMutableContext());

        // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
        if(context->webSocketConn())
        {
            disconnectWebsocketCallback_(context->webSocketConn());
        }
        conn->setContext(std::string("None"));
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           MsgBuffer* buf)
{
    HttpContext* context = any_cast<HttpContext>(conn->getMutableContext());

    // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
    if(context->webSocketConn())
    {
        //websocket payload,we shouldn't parse it
        webSocketMessageCallback_(context->webSocketConn(),buf);
        return;
    }
    if (!context->parseRequest(buf)) {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        //conn->shutdown();
    }

    if (context->gotAll()) {
        context->requestImpl()->parsePremeter();
        context->requestImpl()->setPeerAddr(conn->peerAddr());
        context->requestImpl()->setLocalAddr(conn->localAddr());
        context->requestImpl()->setReceiveDate(trantor::Date::date());
        if(context->firstReq()&&isWebSocket(conn,context->request()))
        {
            auto wsConn=std::make_shared<WebSocketConnectionImpl>(conn);
            newWebsocketCallback_(context->request(),[=](HttpResponse &resp) mutable
            {
                if(resp.statusCode()==HttpResponse::k101)
                {
                    context->setWebsockConnection(wsConn);
                }
                MsgBuffer buffer;
                ((HttpResponseImpl &)resp).appendToBuffer(&buffer);
                conn->send(buffer.peek(),buffer.readableBytes());
            },wsConn);
        }
        else
            onRequest(conn, context->request());
        context->reset();
    }
}
bool HttpServer::isWebSocket(const TcpConnectionPtr& conn, const HttpRequestPtr& req)
{
    if(req->getHeader("Connection")=="Upgrade"&&
       req->getHeader("Upgrade")=="websocket")
    {
        LOG_TRACE<<"new websocket request";

        return true;
    }
    return false;
}
void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequestPtr& req)
{
    const std::string& connection = req->getHeader("Connection");
    bool _close = connection == "close" ||
                  (req->getVersion() == HttpRequestImpl::kHttp10 && connection != "Keep-Alive");



    httpAsyncCallback_(req, [ = ](HttpResponse & response) {
        MsgBuffer buf;
        response.setCloseConnection(_close);
        ((HttpResponseImpl &)response).appendToBuffer(&buf);
        conn->send(buf.peek(),buf.readableBytes());
//        if (response.closeConnection()) {
//            conn->shutdown();
//        }
    });


}

