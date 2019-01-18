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
#include "HttpAppFrameworkImpl.h"
#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include "ssl_funcs/Sha1.h"
#endif
using namespace drogon;

void WebsocketControllersRouter::registerWebSocketController(const std::string &pathName,
                                                             const std::string &ctrlName,
                                                             const std::vector<std::string> &filters)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
    auto objPtr = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(ctrlName));
    auto ctrlPtr = std::dynamic_pointer_cast<WebSocketControllerBase>(objPtr);
    assert(ctrlPtr);
    std::lock_guard<std::mutex> guard(_websockCtrlMutex);

    _websockCtrlMap[path].controller = ctrlPtr;
    _websockCtrlMap[path].filtersName = filters;
}

void WebsocketControllersRouter::route(const HttpRequestImplPtr &req,
                                       const std::shared_ptr<std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                                       const WebSocketConnectionPtr &wsConnPtr)
{
    std::string wsKey = req->getHeaderBy("sec-websocket-key");
    if (!wsKey.empty())
    {
        // magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        WebSocketControllerBasePtr ctrlPtr;
        std::vector<std::string> filtersName;
        {
            std::string pathLower(req->path());
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), tolower);
            std::lock_guard<std::mutex> guard(_websockCtrlMutex);
            if (_websockCtrlMap.find(pathLower) != _websockCtrlMap.end())
            {
                ctrlPtr = _websockCtrlMap[pathLower].controller;
                filtersName = _websockCtrlMap[pathLower].filtersName;
            }
        }
        if (ctrlPtr)
        {
            if (!filtersName.empty())
            {
                _appImpl.doFilters(filtersName, req, callbackPtr, false, nullptr, [=]() mutable {
                    doControllerHandler(ctrlPtr, wsKey, req, *callbackPtr, wsConnPtr);
                });
            }
            else
            {
                doControllerHandler(ctrlPtr, wsKey, req, *callbackPtr, wsConnPtr);
            }
            return;
        }
    }
    auto resp = drogon::HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    (*callbackPtr)(resp);
}

void WebsocketControllersRouter::doControllerHandler(const WebSocketControllerBasePtr &ctrlPtr,
                                                     std::string &wsKey,
                                                     const HttpRequestImplPtr &req,
                                                     const std::function<void(const HttpResponsePtr &)> &callback,
                                                     const WebSocketConnectionPtr &wsConnPtr)
{
    wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    unsigned char accKey[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()), wsKey.length(), accKey);
    auto base64Key = base64Encode(accKey, SHA_DIGEST_LENGTH);
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
    resp->addHeader("Upgrade", "websocket");
    resp->addHeader("Connection", "Upgrade");
    resp->addHeader("Sec-WebSocket-Accept", base64Key);
    callback(resp);
    auto wsConnImplPtr = std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
    assert(wsConnImplPtr);
    wsConnImplPtr->setController(ctrlPtr);
    ctrlPtr->handleNewConnection(req, wsConnPtr);
    return;
}