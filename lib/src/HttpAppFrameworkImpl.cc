/**
 *
 *  @file HttpAppFrameworkImpl.cc
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

#include "HttpAppFrameworkImpl.h"
#include "HttpRequestImpl.h"
#include "HttpClientImpl.h"
#include "HttpResponseImpl.h"
#include "WebSocketConnectionImpl.h"
#include "StaticFileRouter.h"
#include "HttpSimpleControllersRouter.h"
#include "HttpControllersRouter.h"
#include "WebsocketControllersRouter.h"
#include "HttpClientImpl.h"
#include "AOPAdvice.h"
#include "ConfigLoader.h"
#include "HttpServer.h"
#include "PluginsManager.h"
#include "ListenerManager.h"
#include "SharedLibManager.h"
#include "SessionManager.h"
#include "DbClientManager.h"
#include <drogon/config.h>
#include <algorithm>
#include <drogon/version.h>
#include <drogon/CacheMap.h>
#include <drogon/DrClassMap.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/Session.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <json/json.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <tuple>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <sys/file.h>
#include <uuid.h>
#include <unistd.h>
#else
#include <io.h>
#endif

using namespace drogon;
using namespace std::placeholders;

HttpAppFrameworkImpl::HttpAppFrameworkImpl()
    : staticFileRouterPtr_(new StaticFileRouter{}),
      httpCtrlsRouterPtr_(new HttpControllersRouter(*staticFileRouterPtr_,
                                                    postRoutingAdvices_,
                                                    postRoutingObservers_,
                                                    preHandlingAdvices_,
                                                    preHandlingObservers_,
                                                    postHandlingAdvices_)),
      httpSimpleCtrlsRouterPtr_(
          new HttpSimpleControllersRouter(*httpCtrlsRouterPtr_,
                                          postRoutingAdvices_,
                                          postRoutingObservers_,
                                          preHandlingAdvices_,
                                          preHandlingObservers_,
                                          postHandlingAdvices_)),
      websockCtrlsRouterPtr_(
          new WebsocketControllersRouter(postRoutingAdvices_,
                                         postRoutingObservers_)),
      listenerManagerPtr_(new ListenerManager),
      pluginsManagerPtr_(new PluginsManager),
      dbClientManagerPtr_(new orm::DbClientManager),
      uploadPath_(rootPath_ + "uploads")
{
}

static std::function<void()> f = [] {
    LOG_TRACE << "Initialize the main event loop in the main thread";
};

/// Make sure that the main event loop is initialized in the main thread.
drogon::InitBeforeMainFunction drogon::HttpAppFrameworkImpl::initFirst_([]() {
    HttpAppFrameworkImpl::instance().getLoop()->runInLoop(f);
});

namespace drogon
{
std::string getVersion()
{
    return DROGON_VERSION;
}

std::string getGitCommit()
{
    return DROGON_VERSION_SHA1;
}

HttpResponsePtr defaultErrorHandler(HttpStatusCode code)
{
    return std::make_shared<HttpResponseImpl>(code, CT_TEXT_HTML);
}

static void godaemon(void)
{
    printf("Initializing daemon mode\n");
#ifndef _WIN32
    if (getppid() != 1)
    {
        pid_t pid;
        pid = fork();
        if (pid > 0)
            exit(0);  // parent
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        setsid();
    }

    // redirect stdin/stdout/stderr to /dev/null
    close(0);
    close(1);
    close(2);

    int ret = open("/dev/null", O_RDWR);
    (void)ret;
    ret = dup(0);
    (void)ret;
    ret = dup(0);
    (void)ret;
    umask(0);
#else
    LOG_ERROR << "Cannot run as daemon in Windows";
    exit(1);
#endif

    return;
}

static void TERMFunction(int sig)
{
    if (sig == SIGTERM)
    {
        LOG_WARN << "SIGTERM signal received.";
        HttpAppFrameworkImpl::instance().getTermSignalHandler()();
    }
}

}  // namespace drogon

HttpAppFrameworkImpl::~HttpAppFrameworkImpl() noexcept
{
// Destroy the following objects before the loop destruction
#ifndef _WIN32
    sharedLibManagerPtr_.reset();
#endif
    sessionManagerPtr_.reset();
}
HttpAppFramework &HttpAppFrameworkImpl::setStaticFilesCacheTime(int cacheTime)
{
    staticFileRouterPtr_->setStaticFilesCacheTime(cacheTime);
    return *this;
}
int HttpAppFrameworkImpl::staticFilesCacheTime() const
{
    return staticFileRouterPtr_->staticFilesCacheTime();
}
HttpAppFramework &HttpAppFrameworkImpl::setGzipStatic(bool useGzipStatic)
{
    staticFileRouterPtr_->setGzipStatic(useGzipStatic);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setBrStatic(bool useGzipStatic)
{
    staticFileRouterPtr_->setBrStatic(useGzipStatic);
    return *this;
}
#ifndef _WIN32
HttpAppFramework &HttpAppFrameworkImpl::enableDynamicViewsLoading(
    const std::vector<std::string> &libPaths,
    const std::string &outputPath)
{
    assert(!running_);

    for (auto const &libpath : libPaths)
    {
        if (libpath[0] == '/' ||
            (libpath.length() >= 2 && libpath[0] == '.' && libpath[1] == '/') ||
            (libpath.length() >= 3 && libpath[0] == '.' && libpath[1] == '.' &&
             libpath[2] == '/') ||
            libpath == "." || libpath == "..")
        {
            libFilePaths_.push_back(libpath);
        }
        else
        {
            if (rootPath_[rootPath_.length() - 1] == '/')
                libFilePaths_.push_back(rootPath_ + libpath);
            else
                libFilePaths_.push_back(rootPath_ + "/" + libpath);
        }
    }
    libFileOutputPath_ = outputPath;
    if (!libFileOutputPath_.empty())
    {
        if (drogon::utils::createPath(libFileOutputPath_) == -1)
        {
            LOG_FATAL << "Can't create " << libFileOutputPath_
                      << " path for dynamic views";
            exit(-1);
        }
    }

    return *this;
}
#endif
HttpAppFramework &HttpAppFrameworkImpl::setFileTypes(
    const std::vector<std::string> &types)
{
    staticFileRouterPtr_->setFileTypes(types);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &filtersAndMethods)
{
    assert(!running_);
    websockCtrlsRouterPtr_->registerWebSocketController(pathName,
                                                        ctrlName,
                                                        filtersAndMethods);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::registerHttpSimpleController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &filtersAndMethods)
{
    assert(!running_);
    httpSimpleCtrlsRouterPtr_->registerHttpSimpleController(pathName,
                                                            ctrlName,
                                                            filtersAndMethods);
    return *this;
}

void HttpAppFrameworkImpl::registerHttpController(
    const std::string &pathPattern,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &filters,
    const std::string &handlerName)
{
    assert(!pathPattern.empty());
    assert(binder);
    assert(!running_);
    httpCtrlsRouterPtr_->addHttpPath(
        pathPattern, binder, validMethods, filters, handlerName);
}

void HttpAppFrameworkImpl::registerHttpControllerViaRegex(
    const std::string &regExp,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &filters,
    const std::string &handlerName)
{
    assert(!regExp.empty());
    assert(binder);
    assert(!running_);
    httpCtrlsRouterPtr_->addHttpRegex(
        regExp, binder, validMethods, filters, handlerName);
}

HttpAppFramework &HttpAppFrameworkImpl::setThreadNum(size_t threadNum)
{
    if (threadNum == 0)
    {
        threadNum_ = std::thread::hardware_concurrency();
        return *this;
    }
    threadNum_ = threadNum;
    return *this;
}
PluginBase *HttpAppFrameworkImpl::getPlugin(const std::string &name)
{
    return pluginsManagerPtr_->getPlugin(name);
}
HttpAppFramework &HttpAppFrameworkImpl::addListener(const std::string &ip,
                                                    uint16_t port,
                                                    bool useSSL,
                                                    const std::string &certFile,
                                                    const std::string &keyFile,
                                                    bool useOldTLS)
{
    assert(!running_);
    listenerManagerPtr_->addListener(
        ip, port, useSSL, certFile, keyFile, useOldTLS);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setMaxConnectionNum(
    size_t maxConnections)
{
    maxConnectionNum_ = maxConnections;
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setMaxConnectionNumPerIP(
    size_t maxConnectionsPerIP)
{
    maxConnectionNumPerIP_ = maxConnectionsPerIP;
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::loadConfigFile(
    const std::string &fileName)
{
    ConfigLoader loader(fileName);
    loader.load();
    jsonConfig_ = loader.jsonValue();
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::loadConfigJson(const Json::Value &data)
{
    ConfigLoader loader(data);
    loader.load();
    jsonConfig_ = loader.jsonValue();
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::loadConfigJson(Json::Value &&data)
{
    ConfigLoader loader(std::move(data));
    loader.load();
    jsonConfig_ = loader.jsonValue();
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setLogPath(
    const std::string &logPath,
    const std::string &logfileBaseName,
    size_t logfileSize)
{
    if (logPath == "")
        return *this;
#ifdef _WIN32
    if (_access(logPath.c_str(), 0) != 0)
#else
    if (access(logPath.c_str(), 0) != 0)
#endif
    {
        std::cerr << "Log path does not exist!\n";
        exit(1);
    }
#ifdef _WIN32
    if (_access(logPath.c_str(), 06) != 0)
#else
    if (access(logPath.c_str(), R_OK | W_OK) != 0)
#endif
    {
        std::cerr << "Unable to access log path!\n";
        exit(1);
    }
    logPath_ = logPath;
    logfileBaseName_ = logfileBaseName;
    logfileSize_ = logfileSize;
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setLogLevel(
    trantor::Logger::LogLevel level)
{
    trantor::Logger::setLogLevel(level);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setSSLFiles(const std::string &certPath,
                                                    const std::string &keyPath)
{
    sslCertPath_ = certPath;
    sslKeyPath_ = keyPath;
    return *this;
}

void HttpAppFrameworkImpl::run()
{
    if (!getLoop()->isInLoopThread())
    {
        getLoop()->moveToCurrentThread();
    }
    LOG_TRACE << "Start to run...";
    trantor::AsyncFileLogger asyncFileLogger;
    // Create dirs for cache files
    for (int i = 0; i < 256; ++i)
    {
        char dirName[4];
        snprintf(dirName, sizeof(dirName), "%02x", i);
        std::transform(dirName, dirName + 2, dirName, toupper);
        utils::createPath(getUploadPath() + "/tmp/" + dirName);
    }
    if (runAsDaemon_)
    {
        // go daemon!
        godaemon();
#ifdef __linux__
        getLoop()->resetTimerQueue();
#endif
        getLoop()->resetAfterFork();
    }
    // set relaunching
    if (relaunchOnError_)
    {
#ifndef _WIN32
        while (true)
        {
            int child_status = 0;
            auto child_pid = fork();
            if (child_pid < 0)
            {
                LOG_ERROR << "fork error";
                abort();
            }
            else if (child_pid == 0)
            {
                // child
                break;
            }
            waitpid(child_pid, &child_status, 0);
            sleep(1);
            LOG_INFO << "start new process";
        }
        getLoop()->resetAfterFork();
#endif
    }
    signal(SIGTERM, TERMFunction);
    // set logger
    if (!logPath_.empty())
    {
#ifdef _WIN32
        if (_access(logPath_.c_str(), 06) != 0)
#else
        if (access(logPath_.c_str(), R_OK | W_OK) != 0)
#endif
        {
            LOG_ERROR << "log file path not exist";
            abort();
        }
        else
        {
            std::string baseName = logfileBaseName_;
            if (baseName == "")
            {
                baseName = "drogon";
            }
            asyncFileLogger.setFileName(baseName, ".log", logPath_);
            asyncFileLogger.startLogging();
            trantor::Logger::setOutputFunction(
                [&](const char *msg, const uint64_t len) {
                    asyncFileLogger.output(msg, len);
                },
                [&]() { asyncFileLogger.flush(); });
            asyncFileLogger.setFileSizeLimit(logfileSize_);
        }
    }
    if (relaunchOnError_)
    {
        LOG_INFO << "Start child process";
    }
    // now start runing!!

    running_ = true;
#ifndef _WIN32
    if (!libFilePaths_.empty())
    {
        sharedLibManagerPtr_ = std::unique_ptr<SharedLibManager>(
            new SharedLibManager(libFilePaths_, libFileOutputPath_));
    }
#endif
    // Create all listeners.
    auto ioLoops = listenerManagerPtr_->createListeners(
        std::bind(&HttpAppFrameworkImpl::onAsyncRequest, this, _1, _2),
        std::bind(&HttpAppFrameworkImpl::onNewWebsockRequest, this, _1, _2, _3),
        std::bind(&HttpAppFrameworkImpl::onConnection, this, _1),
        idleConnectionTimeout_,
        sslCertPath_,
        sslKeyPath_,
        threadNum_,
        syncAdvices_);
    assert(ioLoops.size() == threadNum_);
    for (size_t i = 0; i < threadNum_; ++i)
    {
        ioLoops[i]->setIndex(i);
    }
    getLoop()->setIndex(threadNum_);
    // A fast database client instance should be created in the main event
    // loop, so put the main loop into ioLoops.
    ioLoops.push_back(getLoop());
    dbClientManagerPtr_->createDbClients(ioLoops);
    httpCtrlsRouterPtr_->init(ioLoops);
    httpSimpleCtrlsRouterPtr_->init(ioLoops);
    staticFileRouterPtr_->init(ioLoops);
    websockCtrlsRouterPtr_->init();

    if (useSession_)
    {
        sessionManagerPtr_ = std::unique_ptr<SessionManager>(
            new SessionManager(getLoop(), sessionTimeout_));
    }

    // Initialize plugins
    const auto &pluginConfig = jsonConfig_["plugins"];
    if (!pluginConfig.isNull())
    {
        pluginsManagerPtr_->initializeAllPlugins(pluginConfig,
                                                 [](PluginBase *plugin) {
                                                     LOG_TRACE
                                                         << "new plugin:"
                                                         << plugin->className();
                                                     // TODO: new plugin
                                                 });
    }
    getLoop()->queueInLoop([this]() {
        // Let listener event loops run when everything is ready.
        listenerManagerPtr_->startListening();
        for (auto &adv : beginningAdvices_)
        {
            adv();
        }
        beginningAdvices_.clear();
    });
    getLoop()->loop();
}

void HttpAppFrameworkImpl::onConnection(const trantor::TcpConnectionPtr &conn)
{
    static std::mutex mtx;
    LOG_TRACE << "connect!!!" << maxConnectionNum_
              << " num=" << connectionNum_.load();
    if (conn->connected())
    {
        if (connectionNum_.fetch_add(1, std::memory_order_relaxed) >=
            maxConnectionNum_)
        {
            LOG_ERROR << "too much connections!force close!";
            conn->forceClose();
            return;
        }
        else if (maxConnectionNumPerIP_ > 0)
        {
            {
                std::lock_guard<std::mutex> lock(mtx);
                auto iter = connectionsNumMap_.find(conn->peerAddr().toIp());
                if (iter == connectionsNumMap_.end())
                {
                    connectionsNumMap_[conn->peerAddr().toIp()] = 1;
                }
                else if (iter->second++ > maxConnectionNumPerIP_)
                {
                    conn->getLoop()->queueInLoop(
                        [conn]() { conn->forceClose(); });
                    return;
                }
            }
        }
        for (auto &advice : newConnectionAdvices_)
        {
            if (!advice(conn->peerAddr(), conn->localAddr()))
            {
                conn->forceClose();
                return;
            }
        }
    }
    else
    {
        if (!conn->hasContext())
        {
            // If the connection is connected to the SSL port and then
            // disconnected before the SSL handshake.
            return;
        }
        connectionNum_.fetch_sub(1, std::memory_order_relaxed);
        if (maxConnectionNumPerIP_ > 0)
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto iter = connectionsNumMap_.find(conn->peerAddr().toIp());
            if (iter != connectionsNumMap_.end())
            {
                --iter->second;
                if (iter->second <= 0)
                {
                    connectionsNumMap_.erase(iter);
                }
            }
        }
    }
}

HttpAppFramework &HttpAppFrameworkImpl::setUploadPath(
    const std::string &uploadPath)
{
    assert(!uploadPath.empty());
    if (uploadPath[0] == '/' ||
        (uploadPath.length() >= 2 && uploadPath[0] == '.' &&
         uploadPath[1] == '/') ||
        (uploadPath.length() >= 3 && uploadPath[0] == '.' &&
         uploadPath[1] == '.' && uploadPath[2] == '/') ||
        uploadPath == "." || uploadPath == "..")
    {
        uploadPath_ = uploadPath;
    }
    else
    {
        if (rootPath_[rootPath_.length() - 1] == '/')
            uploadPath_ = rootPath_ + uploadPath;
        else
            uploadPath_ = rootPath_ + "/" + uploadPath;
    }
    return *this;
}
void HttpAppFrameworkImpl::findSessionForRequest(const HttpRequestImplPtr &req)
{
    if (useSession_)
    {
        std::string sessionId = req->getCookie("JSESSIONID");
        bool needSetJsessionid = false;
        if (sessionId.empty())
        {
            sessionId = utils::getUuid();
            needSetJsessionid = true;
        }
        req->setSession(
            sessionManagerPtr_->getSession(sessionId, needSetJsessionid));
    }
}
void HttpAppFrameworkImpl::onNewWebsockRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    findSessionForRequest(req);
    // Route to controller
    if (!preRoutingObservers_.empty())
    {
        for (auto &observer : preRoutingObservers_)
        {
            observer(req);
        }
    }
    if (preRoutingAdvices_.empty())
    {
        websockCtrlsRouterPtr_->route(req, std::move(callback), wsConnPtr);
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        doAdvicesChain(
            preRoutingAdvices_,
            0,
            req,
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                [req, callbackPtr, this](const HttpResponsePtr &resp) {
                    callCallback(req, resp, *callbackPtr);
                }),
            [this, callbackPtr, req, wsConnPtr]() {
                websockCtrlsRouterPtr_->route(req,
                                              std::move(*callbackPtr),
                                              wsConnPtr);
            });
    }
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpAppFrameworkImpl::getHandlersInfo() const
{
    auto ret = httpSimpleCtrlsRouterPtr_->getHandlersInfo();
    auto v = httpCtrlsRouterPtr_->getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    v = websockCtrlsRouterPtr_->getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    return ret;
}
void HttpAppFrameworkImpl::callCallback(
    const HttpRequestImplPtr &req,
    const HttpResponsePtr &resp,
    const std::function<void(const HttpResponsePtr &)> &callback)
{
    if (useSession_)
    {
        auto &sessionPtr = req->getSession();
        assert(sessionPtr);
        if (sessionPtr->needToChangeSessionId())
        {
            sessionManagerPtr_->changeSessionId(sessionPtr);
        }
        if (sessionPtr->needSetToClient())
        {
            if (resp->expiredTime() >= 0)
            {
                auto newResp = std::make_shared<HttpResponseImpl>(
                    *static_cast<HttpResponseImpl *>(resp.get()));
                newResp->setExpiredTime(-1);  // make it temporary
                auto jsessionid = Cookie("JSESSIONID", sessionPtr->sessionId());
                jsessionid.setPath("/");
                newResp->addCookie(std::move(jsessionid));
                sessionPtr->hasSet();
                callback(newResp);
                return;
            }
            else
            {
                auto jsessionid = Cookie("JSESSIONID", sessionPtr->sessionId());
                jsessionid.setPath("/");
                resp->addCookie(std::move(jsessionid));
                sessionPtr->hasSet();
                callback(resp);
                return;
            }
        }
        else if (resp->version() != req->version())
        {
            auto newResp = std::make_shared<HttpResponseImpl>(
                *static_cast<HttpResponseImpl *>(resp.get()));
            newResp->setVersion(req->version());
            newResp->setExpiredTime(-1);  // make it temporary
            callback(newResp);
            return;
        }
        else
        {
            callback(resp);
            return;
        }
    }
    else
    {
        if (resp->expiredTime() >= 0 && resp->version() != req->version())
        {
            auto newResp = std::make_shared<HttpResponseImpl>(
                *static_cast<HttpResponseImpl *>(resp.get()));
            newResp->setVersion(req->version());
            newResp->setExpiredTime(-1);  // make it temporary
            callback(newResp);
            return;
        }
        else
        {
            callback(resp);
        }
    }
}
void HttpAppFrameworkImpl::onAsyncRequest(
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
        resp->addHeader("ALLOW", "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH");
        resp->setExpiredTime(0);
        callback(resp);
        return;
    }
    findSessionForRequest(req);
    // Route to controller
    if (!preRoutingObservers_.empty())
    {
        for (auto &observer : preRoutingObservers_)
        {
            observer(req);
        }
    }
    if (preRoutingAdvices_.empty())
    {
        httpSimpleCtrlsRouterPtr_->route(req, std::move(callback));
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        doAdvicesChain(
            preRoutingAdvices_,
            0,
            req,
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                [req, callbackPtr, this](const HttpResponsePtr &resp) {
                    callCallback(req, resp, *callbackPtr);
                }),
            [this, callbackPtr, req]() {
                httpSimpleCtrlsRouterPtr_->route(req, std::move(*callbackPtr));
            });
    }
}

trantor::EventLoop *HttpAppFrameworkImpl::getLoop() const
{
    static trantor::EventLoop loop;
    return &loop;
}

trantor::EventLoop *HttpAppFrameworkImpl::getIOLoop(size_t id) const
{
    assert(listenerManagerPtr_);
    return listenerManagerPtr_->getIOLoop(id);
}

HttpAppFramework &HttpAppFramework::instance()
{
    return HttpAppFrameworkImpl::instance();
}

HttpAppFramework::~HttpAppFramework()
{
}
void HttpAppFrameworkImpl::forward(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &hostString,
    double timeout)
{
    forward(std::dynamic_pointer_cast<HttpRequestImpl>(req),
            std::move(callback),
            hostString,
            timeout);
}
void HttpAppFrameworkImpl::forward(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &hostString,
    double timeout)
{
    if (hostString.empty())
    {
        onAsyncRequest(req, std::move(callback));
    }
    else
    {
        /// A tiny implementation of a reverse proxy.
        static std::unordered_map<std::string, HttpClientImplPtr> clientsMap;
        HttpClientImplPtr clientPtr;
        static std::mutex mtx;
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto iter = clientsMap.find(hostString);
            if (iter != clientsMap.end())
            {
                clientPtr = iter->second;
            }
            else
            {
                clientPtr = std::make_shared<HttpClientImpl>(
                    trantor::EventLoop::getEventLoopOfCurrentThread()
                        ? trantor::EventLoop::getEventLoopOfCurrentThread()
                        : getLoop(),
                    hostString);
                clientsMap[hostString] = clientPtr;
            }
        }
        req->setPassThrough(true);
        clientPtr->sendRequest(
            req,
            [callback = std::move(callback)](ReqResult result,
                                             const HttpResponsePtr &resp) {
                if (result == ReqResult::Ok)
                {
                    resp->setPassThrough(true);
                    callback(resp);
                }
                else
                {
                    callback(HttpResponse::newNotFoundResponse());
                }
            },
            timeout);
    }
}

orm::DbClientPtr HttpAppFrameworkImpl::getDbClient(const std::string &name)
{
    return dbClientManagerPtr_->getDbClient(name);
}
orm::DbClientPtr HttpAppFrameworkImpl::getFastDbClient(const std::string &name)
{
    return dbClientManagerPtr_->getFastDbClient(name);
}
HttpAppFramework &HttpAppFrameworkImpl::createDbClient(
    const std::string &dbType,
    const std::string &host,
    const unsigned short port,
    const std::string &databaseName,
    const std::string &userName,
    const std::string &password,
    const size_t connectionNum,
    const std::string &filename,
    const std::string &name,
    const bool isFast,
    const std::string &characterSet)
{
    assert(!running_);
    dbClientManagerPtr_->createDbClient(dbType,
                                        host,
                                        port,
                                        databaseName,
                                        userName,
                                        password,
                                        connectionNum,
                                        filename,
                                        name,
                                        isFast,
                                        characterSet);
    return *this;
}

void HttpAppFrameworkImpl::quit()
{
    if (getLoop()->isRunning())
    {
        getLoop()->queueInLoop([this]() {
            listenerManagerPtr_->stopListening();
            getLoop()->quit();
        });
    }
}

const HttpResponsePtr &HttpAppFrameworkImpl::getCustom404Page()
{
    if (!custom404_)
    {
        return custom404_;
    }
    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
    if (loop && loop->index() < app().getThreadNum())
    {
        // If the current thread is an IO thread
        static IOThreadStorage<HttpResponsePtr> thread404Pages;
        static std::once_flag once;
        std::call_once(once, [this] {
            thread404Pages.init([this](HttpResponsePtr &resp, size_t index) {
                resp = std::make_shared<HttpResponseImpl>(
                    *static_cast<HttpResponseImpl *>(custom404_.get()));
            });
        });
        return thread404Pages.getThreadData();
    }
    else
    {
        return custom404_;
    }
}

HttpAppFramework &HttpAppFrameworkImpl::setStaticFileHeaders(
    const std::vector<std::pair<std::string, std::string>> &headers)
{
    staticFileRouterPtr_->setStaticFileHeaders(headers);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::addALocation(
    const std::string &uriPrefix,
    const std::string &defaultContentType,
    const std::string &alias,
    bool isCaseSensitive,
    bool allowAll,
    bool isRecursive,
    const std::vector<std::string> &filters)
{
    staticFileRouterPtr_->addALocation(uriPrefix,
                                       defaultContentType,
                                       alias,
                                       isCaseSensitive,
                                       allowAll,
                                       isRecursive,
                                       filters);
    return *this;
}

bool HttpAppFrameworkImpl::areAllDbClientsAvailable() const noexcept
{
    return dbClientManagerPtr_->areAllDbClientsAvailable();
}

HttpAppFramework &HttpAppFrameworkImpl::setCustomErrorHandler(
    std::function<HttpResponsePtr(HttpStatusCode)> &&resp_generator)
{
    customErrorHandler_ = std::move(resp_generator);
    usingCustomErrorHandler_ = true;
    return *this;
}

const std::function<HttpResponsePtr(HttpStatusCode)>
    &HttpAppFrameworkImpl::getCustomErrorHandler() const
{
    return customErrorHandler_;
}

std::vector<trantor::InetAddress> HttpAppFrameworkImpl::getListeners() const
{
    return listenerManagerPtr_->getListeners();
}
