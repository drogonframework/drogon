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
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <functional>
#include <memory>
#include <utility>
#include "AOPAdvice.h"
#include "FiltersFunction.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpConnectionLimit.h"
#include "HttpControllerBinder.h"
#include "HttpRequestImpl.h"
#include "HttpRequestParser.h"
#include "HttpResponseImpl.h"
#include "HttpControllersRouter.h"
#include "StaticFileRouter.h"
#include "WebSocketConnectionImpl.h"

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

static inline bool isWebSocket(const HttpRequestImplPtr &req);
static inline HttpResponsePtr tryDecompressRequest(
    const HttpRequestImplPtr &req);
static inline bool passSyncAdvices(
    const HttpRequestImplPtr &req,
    const std::shared_ptr<HttpRequestParser> &requestParser,
    bool shouldBePipelined,
    bool isHeadMethod);
static inline HttpResponsePtr getCompressedResponse(
    const HttpRequestImplPtr &req,
    const HttpResponsePtr &response,
    bool isHeadMethod);

static void handleInvalidHttpMethod(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback);

static void handleHttpOptions(
    const HttpRequestImplPtr &req,
    const std::string &allowMethods,
    std::function<void(const HttpResponsePtr &)> &&callback);

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       std::string name)
#ifdef __linux__
    : server_(loop, listenAddr, std::move(name))
#else
    : server_(loop, listenAddr, std::move(name), true, app().reusePort())
#endif
{
    server_.setConnectionCallback(onConnection);
    server_.setRecvMessageCallback(onMessage);
    server_.kickoffIdleConnections(
        HttpAppFrameworkImpl::instance().getIdleConnectionTimeout());
}

HttpServer::~HttpServer() = default;

void HttpServer::start()
{
    LOG_TRACE << "HttpServer[" << server_.name() << "] starts listening on "
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
        if (!HttpConnectionLimit::instance().tryAddConnection(conn))
        {
            LOG_ERROR << "too much connections!force close!";
            conn->forceClose();
            return;
        }
        if (!AopAdvice::instance().passNewConnectionAdvices(conn))
        {
            conn->forceClose();
        }
    }
    else if (conn->disconnected())
    {
        LOG_TRACE << "conn disconnected!";
        HttpConnectionLimit::instance().releaseConnection(conn);
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
    if (requestParser->webSocketConn())
    {
        // Websocket payload
        requestParser->webSocketConn()->onNewMessage(conn, buf);
        return;
    }

    auto &requests = requestParser->getRequestBuffer();
    // With the pipelining feature or web socket, it is possible to receive
    // multiple messages at once, so the while loop is necessary
    while (buf->readableBytes() > 0)
    {
        if (requestParser->isStop())
        {
            // The number of requests has reached the limit.
            buf->retrieveAll();
            return;
        }
        int parseRes = requestParser->parseRequest(buf);
        if (parseRes < 0)
        {
            requestParser->reset();
            conn->forceClose();
            return;
        }
        if (parseRes == 0)
        {
            break;
        }
        auto &req = requestParser->requestImpl();
        req->setPeerAddr(conn->peerAddr());
        req->setLocalAddr(conn->localAddr());
        req->setCreationDate(trantor::Date::date());
        req->setSecure(conn->isSSLConnection());
        req->setPeerCertificate(conn->peerCertificate());
        requests.push_back(req);
        requestParser->reset();
    }
    onRequests(conn, requests, requestParser);
    requests.clear();
}

struct CallbackParamPack
{
    CallbackParamPack(trantor::TcpConnectionPtr conn,
                      HttpRequestImplPtr req,
                      std::shared_ptr<bool> loopFlag,
                      std::shared_ptr<HttpRequestParser> requestParser,
                      bool isHeadMethod)
        : conn_(std::move(conn)),
          req_(std::move(req)),
          loopFlag_(std::move(loopFlag)),
          requestParser_(std::move(requestParser)),
          isHeadMethod_(isHeadMethod)
    {
    }

    trantor::TcpConnectionPtr conn_;
    HttpRequestImplPtr req_;
    std::shared_ptr<bool> loopFlag_;
    std::shared_ptr<HttpRequestParser> requestParser_;
    bool isHeadMethod_;
    std::atomic<bool> responseSent_{false};
};

void HttpServer::onRequests(
    const TcpConnectionPtr &conn,
    const std::vector<HttpRequestImplPtr> &requests,
    const std::shared_ptr<HttpRequestParser> &requestParser)
{
    if (requests.empty())
        return;

    // will only be checked for the first request
    if (requestParser->firstReq() && requests.size() == 1 &&
        isWebSocket(requests[0]))
    {
        auto &req = requests[0];
        if (passSyncAdvices(req,
                            requestParser,
                            false /* Not pipelined */,
                            false /* Not HEAD */))
        {
            auto wsConn = std::make_shared<WebSocketConnectionImpl>(conn);
            wsConn->setPingMessage("", std::chrono::seconds{30});
            onWebsocketRequest(
                req,
                [conn, wsConn, requestParser, req](
                    const HttpResponsePtr &resp0) mutable {
                    if (conn->connected())
                    {
                        auto resp = HttpAppFrameworkImpl::instance()
                                        .handleSessionForResponse(req, resp0);
                        AopAdvice::instance().passPreSendingAdvices(req, resp);
                        if (resp->statusCode() == k101SwitchingProtocols)
                        {
                            requestParser->setWebsockConnection(wsConn);
                        }
                        auto httpString =
                            ((HttpResponseImpl *)resp.get())->renderToBuffer();
                        conn->send(httpString);
                        COZ_PROGRESS
                    }
                },
                std::move(wsConn));
            return;
        }

        // flush response for not passing sync advices
        if (conn->connected() && !requestParser->getResponseBuffer().empty())
        {
            sendResponses(conn,
                          requestParser->getResponseBuffer(),
                          requestParser->getBuffer());
            requestParser->getResponseBuffer().clear();
        }
        return;
    }

    if (HttpAppFrameworkImpl::instance().keepaliveRequestsNumber() > 0 &&
        requestParser->numberOfRequestsParsed() >=
            HttpAppFrameworkImpl::instance().keepaliveRequestsNumber())
    {
        requestParser->stop();
        conn->shutdown();
        return;
    }
    if (HttpAppFrameworkImpl::instance().pipeliningRequestsNumber() > 0 &&
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
        bool isHeadMethod = (req->method() == Head);
        if (isHeadMethod)
        {
            req->setMethod(Get);
        }
        bool reqPipelined = false;
        if (!requestParser->emptyPipelining())
        {
            requestParser->pushRequestToPipelining(req, isHeadMethod);
            reqPipelined = true;
        }
        if (!passSyncAdvices(req, requestParser, reqPipelined, isHeadMethod))
        {
            continue;
        }

        // Optimization: Avoids dynamic allocation when copying the callback in
        // handlers (ex: copying callback into lambda captures in DB calls)
        bool respReady{false};
        auto paramPack = std::make_shared<CallbackParamPack>(
            conn, req, loopFlagPtr, requestParser, isHeadMethod);

        auto errResp = tryDecompressRequest(req);
        if (errResp)
        {
            handleResponse(errResp, paramPack, &respReady);
        }
        else
        {
            // `handleResponse()` callback may be called synchronously. In this
            // case, the generated response should not be sent right away, but
            // be queued in buffer instead. Those ready responses will be sent
            // together after the end of the for loop.
            //
            // By doing this, we could reduce some system calls when sending
            // through socket. In order to achieve this, we create a
            // `respReady` variable.
            onHttpRequest(req,
                          [respReadyPtr = &respReady,
                           paramPack = std::move(paramPack)](
                              const HttpResponsePtr &response) {
                              handleResponse(response, paramPack, respReadyPtr);
                          });
        }
        if (!reqPipelined && !respReady)
        {
            requestParser->pushRequestToPipelining(req, isHeadMethod);
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

void HttpServer::onHttpRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    LOG_TRACE << "new request:" << req->peerAddr().toIpPort() << "->"
              << req->localAddr().toIpPort();
    LOG_TRACE << "Headers " << req->methodString() << " " << req->path();
    LOG_TRACE << "http path=" << req->path();
    if (req->method() == Options && (req->path() == "*" || req->path() == "/*"))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
        resp->addHeader("Allow", "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH");
        resp->setExpiredTime(0);
        callback(resp);
        return;
    }

    // TODO: move session related codes to its own singleton class
    HttpAppFrameworkImpl::instance().findSessionForRequest(req);
    // pre-routing aop
    auto &aop = AopAdvice::instance();
    aop.passPreRoutingObservers(req);
    if (!aop.hasPreRoutingAdvices())
    {
        httpRequestRouting(req, std::move(callback));
        return;
    }
    aop.passPreRoutingAdvices(req,
                              [req, callback = std::move(callback)](
                                  const HttpResponsePtr &resp) mutable {
                                  if (resp)
                                  {
                                      callback(resp);
                                  }
                                  else
                                  {
                                      httpRequestRouting(req,
                                                         std::move(callback));
                                  }
                              });
}

void HttpServer::httpRequestRouting(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // How to access router here?? Make router class singleton?
    RouteResult result = HttpControllersRouter::instance().route(req);
    if (result.result == RouteResult::Success)
    {
        HttpRequestParamPack pack{std::move(result.binderPtr),
                                  std::move(callback)};
        requestPostRouting(req, std::move(pack));
        return;
    }
    if (result.result == RouteResult::MethodNotAllowed)
    {
        handleInvalidHttpMethod(req, std::move(callback));
        return;
    }

    // Fallback to static file router
    // TODO: make this router a plugin
    if (req->path() == "/" &&
        !HttpAppFrameworkImpl::instance().getHomePage().empty())
    {
        req->setPath("/" + HttpAppFrameworkImpl::instance().getHomePage());
    }
    StaticFileRouter::instance().route(req, std::move(callback));
}

template <typename Pack>
void HttpServer::requestPostRouting(const HttpRequestImplPtr &req, Pack &&pack)
{
    // post-routing aop
    auto &aop = AopAdvice::instance();
    aop.passPostRoutingObservers(req);
    if (!aop.hasPostRoutingAdvices())
    {
        requestPassFilters(req, std::forward<Pack>(pack));
        return;
    }
    aop.passPostRoutingAdvices(req,
                               [req, pack = std::forward<Pack>(pack)](
                                   const HttpResponsePtr &resp) mutable {
                                   if (resp)
                                   {
                                       pack.callback(resp);
                                   }
                                   else
                                   {
                                       requestPassFilters(req, std::move(pack));
                                   }
                               });
}

template <typename Pack>
void HttpServer::requestPassFilters(const HttpRequestImplPtr &req, Pack &&pack)
{
    // pass filters
    auto &filters = pack.binderPtr->filters_;
    if (filters.empty())
    {
        requestPreHandling(req, std::forward<Pack>(pack));
        return;
    }
    filters_function::doFilters(filters,
                                req,
                                [req, pack = std::forward<Pack>(pack)](
                                    const HttpResponsePtr &resp) mutable {
                                    if (resp)
                                    {
                                        pack.callback(resp);
                                    }
                                    else
                                    {
                                        requestPreHandling(req,
                                                           std::move(pack));
                                    }
                                });
}

template <typename Pack>
void HttpServer::requestPreHandling(const HttpRequestImplPtr &req, Pack &&pack)
{
    if (req->method() == Options)
    {
        handleHttpOptions(req,
                          *pack.binderPtr->corsMethods_,
                          std::move(pack.callback));
        return;
    }

    // pre-handling aop
    auto &aop = AopAdvice::instance();
    aop.passPreHandlingObservers(req);
    if (!aop.hasPreHandlingAdvices())
    {
        if constexpr (std::is_same_v<std::decay_t<Pack>, HttpRequestParamPack>)
        {
            httpRequestHandling(req,
                                std::move(pack.binderPtr),
                                std::move(pack.callback));
        }
        else
        {
            websocketRequestHandling(req,
                                     std::move(pack.binderPtr),
                                     std::move(pack.callback),
                                     std::move(pack.wsConnPtr));
        }
        return;
    }
    aop.passPreHandlingAdvices(
        req,
        [req,
         pack = std::forward<Pack>(pack)](const HttpResponsePtr &resp) mutable {
            if (resp)
            {
                pack.callback(resp);
                return;
            }
            if constexpr (std::is_same_v<std::decay_t<Pack>,
                                         HttpRequestParamPack>)
            {
                httpRequestHandling(req,
                                    std::move(pack.binderPtr),
                                    std::move(pack.callback));
            }
            else
            {
                websocketRequestHandling(req,
                                         std::move(pack.binderPtr),
                                         std::move(pack.callback),
                                         std::move(pack.wsConnPtr));
            }
        });
}

void HttpServer::httpRequestHandling(
    const HttpRequestImplPtr &req,
    std::shared_ptr<ControllerBinderBase> &&binderPtr,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Check cached response
    auto &cachedResp = *(binderPtr->responseCache_);
    if (cachedResp)
    {
        if (cachedResp->expiredTime() == 0 ||
            (trantor::Date::now() <
             cachedResp->creationDate().after(
                 static_cast<double>(cachedResp->expiredTime()))))
        {
            // use cached response!
            LOG_TRACE << "Use cached response";

            // post-handling aop
            AopAdvice::instance().passPostHandlingAdvices(req, cachedResp);
            callback(cachedResp);
            return;
        }
        else
        {
            cachedResp.reset();
        }
    }

    auto &binderRef = *binderPtr;
    binderRef.handleRequest(
        req,
        // This is the actual callback being passed to controller
        [req, binderPtr = std::move(binderPtr), callback = std::move(callback)](
            const HttpResponsePtr &resp) mutable {
            // Check if we need to cache the response
            if (resp->expiredTime() >= 0 && resp->statusCode() != k404NotFound)
            {
                static_cast<HttpResponseImpl *>(resp.get())->makeHeaderString();
                auto loop = req->getLoop();
                if (loop->isInLoopThread())
                {
                    binderPtr->responseCache_.setThreadData(resp);
                }
                else
                {
                    loop->queueInLoop(
                        [binderPtr = std::move(binderPtr), resp]() {
                            binderPtr->responseCache_.setThreadData(resp);
                        });
                }
            }
            // post-handling aop
            AopAdvice::instance().passPostHandlingAdvices(req, resp);
            callback(resp);
        });
}

void HttpServer::onWebsocketRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    WebSocketConnectionImplPtr &&wsConnPtr)
{
    HttpAppFrameworkImpl::instance().findSessionForRequest(req);
    // pre-routing aop
    auto &aop = AopAdvice::instance();
    aop.passPreRoutingObservers(req);
    if (!aop.hasPreRoutingAdvices())
    {
        websocketRequestRouting(req, std::move(callback), std::move(wsConnPtr));
        return;
    }
    aop.passPreRoutingAdvices(
        req,
        [req, wsConnPtr = std::move(wsConnPtr), callback = std::move(callback)](
            const HttpResponsePtr &resp) mutable {
            if (resp)
            {
                callback(resp);
            }
            else
            {
                websocketRequestRouting(req,
                                        std::move(callback),
                                        std::move(wsConnPtr));
            }
        });
}

void HttpServer::websocketRequestRouting(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    WebSocketConnectionImplPtr &&wsConnPtr)
{
    RouteResult result = HttpControllersRouter::instance().routeWs(req);

    if (result.result == RouteResult::Success)
    {
        WsRequestParamPack pack{std::move(result.binderPtr),
                                std::move(callback),
                                std::move(wsConnPtr)};
        requestPostRouting(req, std::move(pack));
        return;
    }
    if (result.result == RouteResult::MethodNotAllowed)
    {
        handleInvalidHttpMethod(req, std::move(callback));
        return;
    }

    // Not found
    auto resp = drogon::HttpResponse::newNotFoundResponse(req);
    resp->setCloseConnection(true);
    callback(resp);
}

void HttpServer::websocketRequestHandling(
    const HttpRequestImplPtr &req,
    std::shared_ptr<ControllerBinderBase> &&binderPtr,
    std::function<void(const HttpResponsePtr &)> &&callback,
    WebSocketConnectionImplPtr &&wsConnPtr)
{
    binderPtr->handleRequest(
        req,
        [req, callback = std::move(callback)](const HttpResponsePtr &resp) {
            AopAdvice::instance().passPostHandlingAdvices(req, resp);
            callback(resp);
        });

    // TODO: more elegant?
    static_cast<WebsocketControllerBinder *>(binderPtr.get())
        ->handleNewConnection(req, wsConnPtr);
}

void HttpServer::handleResponse(
    const HttpResponsePtr &response,
    const std::shared_ptr<CallbackParamPack> &paramPack,
    bool *respReadyPtr)
{
    auto &conn = paramPack->conn_;
    auto &req = paramPack->req_;
    auto &requestParser = paramPack->requestParser_;
    auto &loopFlagPtr = paramPack->loopFlag_;
    const bool isHeadMethod = paramPack->isHeadMethod_;

    if (!response)
        return;
    if (!conn->connected())
        return;

    if (paramPack->responseSent_.exchange(true, std::memory_order_acq_rel))
    {
        LOG_ERROR << "Sending more than 1 response for request. "
                     "Ignoring later response";
        return;
    }

    auto resp =
        HttpAppFrameworkImpl::instance().handleSessionForResponse(req,
                                                                  response);
    resp->setVersion(req->getVersion());
    resp->setCloseConnection(!req->keepAlive());
    AopAdvice::instance().passPreSendingAdvices(req, resp);

    auto newResp = getCompressedResponse(req, resp, isHeadMethod);
    if (conn->getLoop()->isInLoopThread())
    {
        /*
         * A client that supports persistent connections MAY
         * "pipeline" its requests (i.e., send multiple requests
         * without waiting for each response). A server MUST send
         * its responses to those requests in the same order that
         * the requests were received. rfc2616-8.1.1.2
         */
        if (requestParser->emptyPipelining())
        {
            // response must have arrived synchronously
            assert(*loopFlagPtr);
            // TODO: change to weakPtr to be sure. But may drop performance.
            *respReadyPtr = true;
            requestParser->getResponseBuffer().emplace_back(std::move(newResp),
                                                            isHeadMethod);
            return;
        }
        if (requestParser->pushResponseToPipelining(req, std::move(newResp)))
        {
            auto &responseBuffer = requestParser->getResponseBuffer();
            requestParser->popReadyResponses(responseBuffer);
            if (!*loopFlagPtr)
            {
                // We have passed the point where `onRequests()` sends
                // responses. So, at here we should send ready responses from
                // the beginning of pipeline queue.
                sendResponses(conn, responseBuffer, requestParser->getBuffer());
                responseBuffer.clear();
            }
        }
    }
    else
    {
        conn->getLoop()->queueInLoop(
            [conn, req, requestParser, newResp = std::move(newResp)]() mutable {
                if (!conn->connected())
                {
                    return;
                }
                if (requestParser->pushResponseToPipelining(req,
                                                            std::move(newResp)))
                {
                    std::vector<std::pair<HttpResponsePtr, bool>> responses;
                    requestParser->popReadyResponses(responses);
                    sendResponses(conn, responses, requestParser->getBuffer());
                }
            });
    }
}

struct ChunkingParams
{
    using DataCallback = std::function<std::size_t(char *, std::size_t)>;

    explicit ChunkingParams(DataCallback cb) : dataCallback(std::move(cb))
    {
    }

    DataCallback dataCallback;
    bool bFinished{false};
#ifndef NDEBUG  // defined by CMake for release build
    std::size_t nDataReturned{0};
#endif
};

static std::size_t chunkingCallback(
    const std::shared_ptr<ChunkingParams> &cbParams,
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
    // Alternative code if there are client software that do not support chunk
    // size with leading zeroes
    //    auto nHeaderLen =
    // #ifdef _WIN32
    //    sprintf_s(pBuffer,
    //    nHeaderSize, "%llx\r",
    //    nDataSize);
    // #else
    //    sprintf(pBuffer, "%lx\r",
    //    nDataSize);
    // #endif
    //    pBuffer[nHeaderLen++] = '\n';
    //    if (nHeaderLen < nHeaderSize)  // smaller that what was reserved ->
    //    move data
    // #ifdef _WIN32
    //    memmove_s(pBuffer +
    //    nHeaderLen,
    //              nSize - nHeaderLen,
    //              pBuffer +
    //              nHeaderSize,
    //              nDataSize + 2);
    // #else
    //    memmove(pBuffer + nHeaderLen,
    //            pBuffer + nHeaderSize,
    //            nDataSize + 2);
    // #endif
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
        auto &asyncStreamCallback = respImplPtr->asyncStreamCallback();
        if (asyncStreamCallback)
        {
            if (!respImplPtr->ifCloseConnection())
            {
                asyncStreamCallback(
                    std::make_unique<ResponseStream>(conn->sendAsyncStream(
                        respImplPtr->asyncStreamKickoffDisabled())));
            }
            else
            {
                LOG_INFO << "Chunking Set CloseConnection !!!";
            }
        }
        auto &streamCallback = respImplPtr->streamCallback();
        const std::string &sendfileName = respImplPtr->sendfileName();
        if (streamCallback || !sendfileName.empty())
        {
            if (streamCallback)
            {
                auto &headers = respImplPtr->headers();
                // When the transfer-encoding is chunked, wrap data callback
                // chunking callback
                auto bChunked =
                    !respImplPtr->ifCloseConnection() &&
                    (headers.find("transfer-encoding") != headers.end()) &&
                    (headers.at("transfer-encoding") == "chunked");
                if (bChunked)
                {
                    conn->sendStream(
                        [ctx = std::make_shared<ChunkingParams>(
                             streamCallback)](char *buffer, size_t len) {
                            return chunkingCallback(ctx, buffer, len);
                        });
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
            auto &asyncStreamCallback = respImplPtr->asyncStreamCallback();
            if (asyncStreamCallback)
            {
                conn->send(buffer);
                buffer.retrieveAll();
                if (!respImplPtr->ifCloseConnection())
                {
                    asyncStreamCallback(
                        std::make_unique<ResponseStream>(conn->sendAsyncStream(
                            respImplPtr->asyncStreamKickoffDisabled())));
                }
                else
                {
                    LOG_INFO << "Chunking Set CloseConnection !!!";
                }
            }
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
                        conn->sendStream(
                            [ctx = std::make_shared<ChunkingParams>(
                                 streamCallback)](char *buffer, size_t len) {
                                return chunkingCallback(ctx, buffer, len);
                            });
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

static inline bool isWebSocket(const HttpRequestImplPtr &req)
{
    if (req->method() != Get)
        return false;

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

/**
 * @brief calling req->decompressBody(), if not success, generate corresponding
 * error response
 */
static inline HttpResponsePtr tryDecompressRequest(
    const HttpRequestImplPtr &req)
{
    static const bool enableDecompression = app().isCompressedRequestEnabled();
    if (!enableDecompression)
    {
        return nullptr;
    }
    auto status = req->decompressBody();
    if (status == StreamDecompressStatus::Ok)
    {
        return nullptr;
    }
    auto resp = HttpResponse::newHttpResponse();
    switch (status)
    {
        case StreamDecompressStatus::TooLarge:
            resp->setStatusCode(k413RequestEntityTooLarge);
            break;
        case StreamDecompressStatus::DecompressError:
            resp->setStatusCode(k422UnprocessableEntity);
            break;
        case StreamDecompressStatus::NotSupported:
            resp->setStatusCode(k415UnsupportedMediaType);
            break;
        case StreamDecompressStatus::Ok:
            return nullptr;
    }
    return resp;
}

/**
 * @brief Check request against each sync advice, generate response if request
 * is rejected by any one of them.
 *
 * @return true if all sync advices are passed.
 * @return false if rejected by any sync advice.
 */
static inline bool passSyncAdvices(
    const HttpRequestImplPtr &req,
    const std::shared_ptr<HttpRequestParser> &requestParser,
    bool shouldBePipelined,
    bool isHeadMethod)
{
    if (auto resp = AopAdvice::instance().passSyncAdvices(req))
    {
        // Rejected by sync advice
        resp->setVersion(req->getVersion());
        resp->setCloseConnection(!req->keepAlive());
        if (!shouldBePipelined)
        {
            requestParser->getResponseBuffer().emplace_back(
                getCompressedResponse(req, resp, isHeadMethod), isHeadMethod);
        }
        else
        {
            requestParser->pushResponseToPipelining(
                req, getCompressedResponse(req, resp, isHeadMethod));
        }
        return false;
    }
    return true;
}

static inline HttpResponsePtr getCompressedResponse(
    const HttpRequestImplPtr &req,
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
        auto strCompress =
            drogon::utils::brotliCompress(response->getBody().data(),
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
        auto strCompress =
            drogon::utils::gzipCompress(response->getBody().data(),
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

static void handleInvalidHttpMethod(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() != Options)
    {
        callback(app().getCustomErrorHandler()(k405MethodNotAllowed, req));
    }
    else
    {
        callback(app().getCustomErrorHandler()(k403Forbidden, req));
    }
}

static void handleHttpOptions(
    const HttpRequestImplPtr &req,
    const std::string &allowMethods,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
    resp->addHeader("Allow", allowMethods);

    auto &orig = req->getHeaderBy("origin");
    resp->addHeader("Access-Control-Allow-Origin", orig.empty() ? "*" : orig);
    resp->addHeader("Access-Control-Allow-Methods", allowMethods);
    auto &headers = req->getHeaderBy("access-control-request-headers");
    if (!headers.empty())
    {
        resp->addHeader("Access-Control-Allow-Headers", headers);
    }
    callback(resp);
}
