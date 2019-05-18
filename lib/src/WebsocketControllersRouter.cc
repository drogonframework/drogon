/**
 *
 *  WebsocketControllersRouter.cc
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

#include "WebsocketControllersRouter.h"
#include "FiltersFunction.h"
#include "HttpAppFrameworkImpl.h"
#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include "ssl_funcs/Sha1.h"
#endif
using namespace drogon;

void WebsocketControllersRouter::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<std::string> &filters)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
    auto objPtr = DrClassMap::getSingleInstance(ctrlName);
    auto ctrlPtr = std::dynamic_pointer_cast<WebSocketControllerBase>(objPtr);
    assert(ctrlPtr);
    std::lock_guard<std::mutex> guard(_websockCtrlMutex);

    _websockCtrlMap[path]._controller = ctrlPtr;
    _websockCtrlMap[path]._filterNames = filters;
}

void WebsocketControllersRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    if (!wsKey.empty())
    {
        // magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        std::string pathLower(req->path());
        std::transform(pathLower.begin(),
                       pathLower.end(),
                       pathLower.begin(),
                       tolower);
        auto iter = _websockCtrlMap.find(pathLower);
        if (iter != _websockCtrlMap.end())
        {
            auto &ctrlPtr = iter->second._controller;
            auto &filters = iter->second._filters;
            if (ctrlPtr)
            {
                if (!filters.empty())
                {
                    auto callbackPtr = std::make_shared<
                        std::function<void(const HttpResponsePtr &)>>(
                        std::move(callback));
                    FiltersFunction::doFilters(filters,
                                               req,
                                               callbackPtr,
                                               false,
                                               nullptr,
                                               [=]() mutable {
                                                   doControllerHandler(
                                                       ctrlPtr,
                                                       wsKey,
                                                       req,
                                                       std::move(*callbackPtr),
                                                       wsConnPtr);
                                               });
                }
                else
                {
                    doControllerHandler(
                        ctrlPtr, wsKey, req, std::move(callback), wsConnPtr);
                }
                return;
            }
        }
    }
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
WebsocketControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : _websockCtrlMap)
    {
        auto info = std::tuple<std::string, HttpMethod, std::string>(
            item.first,
            Get,
            std::string("WebsocketController: ") +
                item.second._controller->className());
        ret.emplace_back(std::move(info));
    }
    return ret;
}

void WebsocketControllersRouter::doControllerHandler(
    const WebSocketControllerBasePtr &ctrlPtr,
    std::string &wsKey,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()),
         wsKey.length(),
         accKey);
    auto base64Key = utils::base64Encode(accKey, SHA_DIGEST_LENGTH);
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k101SwitchingProtocols);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", base64Key);
    callback(resp);
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
    return;
}

void WebsocketControllersRouter::init()
{
    for (auto &iter : _websockCtrlMap)
    {
        iter.second._filters =
            FiltersFunction::createFilters(iter.second._filterNames);
    }
}