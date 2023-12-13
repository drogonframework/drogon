/**
 *
 *  @file HttpControllerBinder.cc
 *  @author Nitromelon
 *
 *  Copyright 2023, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "HttpControllerBinder.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpSimpleController.h>
#include <drogon/WebSocketController.h>

namespace drogon
{

void HttpSimpleControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    // Binders without controller should be removed after run()
    assert(controller_);

    try
    {
        auto cb = callback;  // copy
        controller_->asyncHandleHttpRequest(req, std::move(cb));
    }
    catch (const std::exception &e)
    {
        app().getExceptionHandler()(e, req, std::move(callback));
        return;
    }
    catch (...)
    {
        LOG_ERROR << "Exception not derived from std::exception";
        return;
    }
}

void HttpControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto &paramsVector = req->getRoutingParameters();
    std::deque<std::string> params(paramsVector.begin(), paramsVector.end());
    binderPtr_->handleHttpRequest(params, req, std::move(callback));
}

void WebsocketControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[20];
    auto sha1 = trantor::utils::sha1(wsKey.c_str(), wsKey.length());
    memcpy(accKey, &sha1, sizeof(sha1));
    auto base64Key = utils::base64Encode(accKey, sizeof(accKey));
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k101SwitchingProtocols);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", base64Key);
    callback(resp);
}

void WebsocketControllerBinder::handleNewConnection(
    const HttpRequestImplPtr &req,
    const WebSocketConnectionImplPtr &wsConnPtr) const
{
    auto ctrlPtr = controller_;
    assert(ctrlPtr);
    wsConnPtr->setMessageCallback(
        [ctrlPtr](std::string &&message,
                  const WebSocketConnectionImplPtr &connPtr,
                  const WebSocketMessageType &type) {
            ctrlPtr->handleNewMessage(connPtr, std::move(message), type);
        });
    wsConnPtr->setCloseCallback(
        [ctrlPtr](const WebSocketConnectionImplPtr &connPtr) {
            ctrlPtr->handleConnectionClosed(connPtr);
        });
    ctrlPtr->handleNewConnection(req, wsConnPtr);
}

}  // namespace drogon
