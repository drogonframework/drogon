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
                   [](unsigned char c) { return tolower(c); });
    auto upgradeField = req->getHeaderBy("upgrade");
    std::transform(upgradeField.begin(),
                   upgradeField.end(),
                   upgradeField.begin(),
                   [](unsigned char c) { return tolower(c); });
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
          isHeadMethod(isHeadMethod_),
          responseSent(false)
    {
    }
    trantor::TcpConnectionPtr conn;
    HttpRequestImplPtr req;
    std::shared_ptr<bool> loopFlag;
    std::shared_ptr<HttpRequestParser> requestParser;
    bool *syncFlagPtr;
    bool close;
    bool isHeadMethod;
    std::atomic<bool> responseSent;
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

        auto handleResponse = [paramPack = std::move(paramPack),
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

            if (paramPack->responseSent.exchange(true,
                                                 std::memory_order_acq_rel) ==
                true)
            {
                LOG_ERROR << "Sending more than 1 response for request. "
                             "Ignoring later response";
                return;
            }

            response->setVersion(req->getVersion());
            response->setCloseConnection(close_);
            for (auto &advice : preSendingAdvices_)
            {
                advice(req, response);
            }
            auto newResp = getCompressedResponse(req, response, isHeadMethod);
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
                        requestParser->pushResponseToPipelining(req,
                                                                newResp,
                                                                isHeadMethod);
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
        };

        const bool enableDecompression = app().isCompressedRequestEnabled();
        bool sendForProcessing = true;
        if (enableDecompression)
        {
            auto status = req->decompressBody();
            if (status != StreamDecompressStatus::Ok)
            {
                sendForProcessing = false;
                auto resp = HttpResponse::newHttpResponse();
                if (status == StreamDecompressStatus::DecompressError)
                    resp->setStatusCode(k422UnprocessableEntity);
                else if (status == StreamDecompressStatus::NotSupported)
                    resp->setStatusCode(k415UnsupportedMediaType);
                else if (status == StreamDecompressStatus::TooLarge)
                    resp->setStatusCode(k413RequestEntityTooLarge);
                else  // Should not happen
                    resp->setStatusCode(k422UnprocessableEntity);
                handleResponse(resp);
            }
        }
        if (sendForProcessing)
            httpAsyncCallback_(req, std::move(handleResponse));
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

using CallbackParams = struct
{
    std::function<std::size_t(char *, std::size_t)> dataCallback;
    bool bFinished;
#ifndef NDEBUG  // defined by CMake for release build
    std::size_t nDataReturned;
#endif
};
static std::size_t chunkingCallback(std::shared_ptr<CallbackParams> cbParams,
                                    char *pBuffer,
                                    std::size_t nSize)
{
    if (!cbParams)
        return 0;
    // Cleanup
    if (pBuffer == nullptr)
    {
        LOG_TRACE << "Chunking callback cleanup";
        if (cbParams && cbParams->dataCallback)
        {
            cbParams->dataCallback(pBuffer, nSize);
            cbParams->dataCallback = {};
            cbParams.reset();
        }
        return 0;
    }
    // Terminal chunk already returned
    if (cbParams->bFinished)
    {
        LOG_TRACE << "Chunking callback has no more data";
#ifndef NDEBUG  // defined by CMake for release build
        LOG_TRACE << "Chunking callback: total data returned: "
                  << cbParams->nDataReturned << " bytes";
#endif
        return 0;
    }
    // Reserve size to prepend the chunk size & append cr/lf, and get data
    struct
    {
        std::size_t operator()(std::size_t n)
        {
            return n == 0 ? 0 : 1 + (*this)(n >> 4);
        }
    } neededDigits;
    auto nHeaderSize = neededDigits(nSize) + 2;
    auto nDataSize =
        cbParams->dataCallback(pBuffer + nHeaderSize, nSize - nHeaderSize - 2);
    if (nDataSize == 0)
    {
        // Terminal chunk + cr/lf
        cbParams->bFinished = true;
#ifdef _WIN32
        memcpy_s(pBuffer, nSize, "0\r\n\r\n", 5);
#else
        memcpy(pBuffer, "0\r\n\r\n", 5);
#endif
        LOG_TRACE << "Chunking callback: no more data, return last chunk of "
                     "size 0 & end of message";
        return 5;
    }
    // Non-terminal chunks
    pBuffer[nHeaderSize + nDataSize] = '\r';
    pBuffer[nHeaderSize + nDataSize + 1] = '\n';
    // The spec does not say if the chunk size is allowed tohave leading zeroes
    // Use a fixed size header with leading zeroes
    // (tested to work with Chrome, Firefox, Safari, Edge, wget, curl and VLC)
#ifdef _WIN32
    char pszFormat[]{"%04llx\r"};
#else
    char pszFormat[]{"%04lx\r"};
#endif
    pszFormat[2] = '0' + char(nHeaderSize - 2);
    snprintf(pBuffer, nHeaderSize, pszFormat, nDataSize);
    pBuffer[nHeaderSize - 1] = '\n';
    LOG_TRACE << "Chunking callback: return chunk of size " << nDataSize;
#ifndef NDEBUG  // defined by CMake for release build
    cbParams->nDataReturned += nDataSize;
#endif
    return nHeaderSize + nDataSize + 2;
    // Alternative code if there are client softwares that do not support chunk
    // size with leading zeroes
    //    auto nHeaderLen =
    //#ifdef _WIN32
    //    sprintf_s(pBuffer,
    //    nHeaderSize, "%llx\r",
    //    nDataSize);
    //#else
    //    sprintf(pBuffer, "%lx\r",
    //    nDataSize);
    //#endif
    //    pBuffer[nHeaderLen++] = '\n';
    //    if (nHeaderLen < nHeaderSize)  // smaller that what was reserved ->
    //    move data
    //#ifdef _WIN32
    //    memmove_s(pBuffer +
    //    nHeaderLen,
    //              nSize - nHeaderLen,
    //              pBuffer +
    //              nHeaderSize,
    //              nDataSize + 2);
    //#else
    //    memmove(pBuffer + nHeaderLen,
    //            pBuffer + nHeaderSize,
    //            nDataSize + 2);
    //#endif
    //    return nHeaderLen + nDataSize + 2;
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
        auto &streamCallback = respImplPtr->streamCallback();
        const std::string &sendfileName = respImplPtr->sendfileName();
        if (streamCallback || !sendfileName.empty())
        {
            if (streamCallback)
            {
                auto &headers = respImplPtr->headers();
                // When the transfer-encoding is chunked, wrap data callback in
                // chunking callback
                auto bChunked =
                    !respImplPtr->ifCloseConnection() &&
                    (headers.find("transfer-encoding") != headers.end()) &&
                    (headers.at("transfer-encoding") == "chunked");
                if (bChunked)
                {
                    auto chunkCallback =
                        std::bind(chunkingCallback,
                                  std::shared_ptr<CallbackParams>(
                                      new CallbackParams{streamCallback,
#ifndef NDEBUG  // defined by CMake for release build
                                                         false,
                                                         0}),
#else
                                                         false}),
#endif

                                  _1,
                                  _2);
                    conn->sendStream(chunkCallback);
                }
                else
                    conn->sendStream(streamCallback);
            }
            else
            {
                const auto &range = respImplPtr->sendfileRange();
                conn->sendFile(sendfileName.c_str(), range.first, range.second);
            }
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
            auto &streamCallback = respImplPtr->streamCallback();
            const std::string &sendfileName = respImplPtr->sendfileName();
            if (streamCallback || !sendfileName.empty())
            {
                conn->send(buffer);
                buffer.retrieveAll();
                if (streamCallback)
                {
                    auto &headers = respImplPtr->headers();
                    // When the transfer-encoding is chunked, encapsulate data
                    // callback in chunking callback
                    auto bChunked =
                        !respImplPtr->ifCloseConnection() &&
                        (headers.find("transfer-encoding") != headers.end()) &&
                        (headers.at("transfer-encoding") == "chunked");
                    if (bChunked)
                    {
                        auto chunkCallback =
                            std::bind(chunkingCallback,
                                      std::shared_ptr<CallbackParams>(
                                          new CallbackParams{streamCallback,
#ifndef NDEBUG  // defined by CMake for release build
                                                             false,
                                                             0}),
#else
                                                             false}),
#endif
                                      _1,
                                      _2);
                        conn->sendStream(chunkCallback);
                    }
                    else
                        conn->sendStream(streamCallback);
                }
                else
                {
                    const auto &range = respImplPtr->sendfileRange();
                    conn->sendFile(sendfileName.c_str(),
                                   range.first,
                                   range.second);
                }
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
