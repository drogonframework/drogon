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
#include <drogon/DrClassMap.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/utils/Utilities.h>
#include <drogon/version.h>
#include <json/json.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <algorithm>
#include "AOPAdvice.h"
#include "ConfigLoader.h"
#include "DbClientManager.h"
#include "HttpClientImpl.h"
#include "HttpControllersRouter.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpServer.h"
#include "HttpSimpleControllersRouter.h"
#include "HttpUtils.h"
#include "ListenerManager.h"
#include "PluginsManager.h"
#include "RedisClientManager.h"
#include "SessionManager.h"
#include "SharedLibManager.h"
#include "StaticFileRouter.h"
#include "WebSocketConnectionImpl.h"
#include "WebsocketControllersRouter.h"
#include "filesystem.h"

#include <iostream>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#define os_access access
#elif !defined(_WIN32) || defined(__MINGW32__)
#include <sys/file.h>
#include <unistd.h>
#define os_access access
#else
#include <io.h>
#define os_access _waccess
#define R_OK 04
#define W_OK 02
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
                                         postRoutingObservers_,
                                         preHandlingAdvices_,
                                         preHandlingObservers_,
                                         postHandlingAdvices_)),
      listenerManagerPtr_(new ListenerManager),
      pluginsManagerPtr_(new PluginsManager),
      dbClientManagerPtr_(new orm::DbClientManager),
      redisClientManagerPtr_(new nosql::RedisClientManager),
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

void defaultExceptionHandler(
    const std::exception &e,
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    std::string pathWithQuery = req->path();
    if (req->query().empty() == false)
        pathWithQuery += "?" + req->query();
    LOG_ERROR << "Unhandled exception in " << pathWithQuery
              << ", what(): " << e.what();
    const auto &handler = app().getCustomErrorHandler();
    callback(handler(k500InternalServerError));
}

static void godaemon()
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
    else if (sig == SIGINT)
    {
        LOG_WARN << "SIGINT signal received.";
        HttpAppFrameworkImpl::instance().getIntSignalHandler()();
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
HttpAppFramework &HttpAppFrameworkImpl::setImplicitPageEnable(
    bool useImplicitPage)
{
    staticFileRouterPtr_->setImplicitPageEnable(useImplicitPage);
    return *this;
}
bool HttpAppFrameworkImpl::isImplicitPageEnabled() const
{
    return staticFileRouterPtr_->isImplicitPageEnabled();
}
HttpAppFramework &HttpAppFrameworkImpl::setImplicitPage(
    const std::string &implicitPageFile)
{
    staticFileRouterPtr_->setImplicitPage(implicitPageFile);
    return *this;
}
const std::string &HttpAppFrameworkImpl::getImplicitPage() const
{
    return staticFileRouterPtr_->getImplicitPage();
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
    assert(!routersInit_);
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
    assert(!routersInit_);
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
    assert(!routersInit_);
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
    assert(!routersInit_);
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

std::shared_ptr<PluginBase> HttpAppFrameworkImpl::getSharedPlugin(
    const std::string &name)
{
    return pluginsManagerPtr_->getSharedPlugin(name);
}
void HttpAppFrameworkImpl::addPlugin(
    const std::string &name,
    const std::vector<std::string> &dependencies,
    const Json::Value &config)
{
    assert(!isRunning());
    Json::Value pluginConfig;
    pluginConfig["name"] = name;
    Json::Value deps(Json::arrayValue);
    for (const auto dep : dependencies)
    {
        deps.append(dep);
    }
    pluginConfig["dependencies"] = deps;
    pluginConfig["config"] = config;
    auto &plugins = jsonRuntimeConfig_["plugins"];
    plugins.append(pluginConfig);
}
void HttpAppFrameworkImpl::addPlugins(const Json::Value &configs)
{
    assert(!isRunning());
    assert(configs.isArray());
    auto &plugins = jsonRuntimeConfig_["plugins"];
    for (const auto config : configs)
    {
        plugins.append(config);
    }
}
HttpAppFramework &HttpAppFrameworkImpl::addListener(
    const std::string &ip,
    uint16_t port,
    bool useSSL,
    const std::string &certFile,
    const std::string &keyFile,
    bool useOldTLS,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
    assert(!running_);
    listenerManagerPtr_->addListener(
        ip, port, useSSL, certFile, keyFile, useOldTLS, sslConfCmds);
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
    size_t logfileSize,
    size_t maxFiles)
{
    if (logPath.empty())
        return *this;
    // std::filesystem does not provide a method to check access permissions, so
    // keep existing code
    if (os_access(utils::toNativePath(logPath).c_str(), 0) != 0)
    {
        std::cerr << "Log path does not exist!\n";
        exit(1);
    }
    if (os_access(utils::toNativePath(logPath).c_str(), R_OK | W_OK) != 0)
    {
        std::cerr << "Unable to access log path!\n";
        exit(1);
    }
    logPath_ = logPath;
    logfileBaseName_ = logfileBaseName;
    logfileSize_ = logfileSize;
    logfileMaxNum_ = maxFiles;
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setLogLevel(
    trantor::Logger::LogLevel level)
{
    trantor::Logger::setLogLevel(level);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setLogLocalTime(bool on)
{
    trantor::Logger::setDisplayLocalTime(on);
    return *this;
}
HttpAppFramework &HttpAppFrameworkImpl::setSSLConfigCommands(
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
    sslConfCmds_ = sslConfCmds;
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
    // Create dirs for cache files
    for (int i = 0; i < 256; ++i)
    {
        char dirName[4];
        snprintf(dirName, sizeof(dirName), "%02x", i);
        std::transform(dirName, dirName + 2, dirName, [](unsigned char c) {
            return toupper(c);
        });
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
#ifdef __linux__
        getLoop()->resetTimerQueue();
#endif
        getLoop()->resetAfterFork();
#endif
    }
    if (handleSigterm_)
    {
#ifdef WIN32
        signal(SIGTERM, TERMFunction);
        signal(SIGINT, TERMFunction);
#else
        struct sigaction sa;
        sa.sa_handler = TERMFunction;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1)
        {
            LOG_ERROR << "sigaction() failed, can't set SIGINT handler";
            abort();
        }
        if (sigaction(SIGTERM, &sa, NULL) == -1)
        {
            LOG_ERROR << "sigaction() failed, can't set SIGTERM handler";
            abort();
        }
#endif
    }
    setupFileLogger();
    if (relaunchOnError_)
    {
        LOG_INFO << "Start child process";
    }

#ifndef _WIN32
    if (!libFilePaths_.empty())
    {
        sharedLibManagerPtr_ =
            std::make_unique<SharedLibManager>(libFilePaths_,
                                               libFileOutputPath_);
    }
#endif

    // Create IO threads
    ioLoopThreadPool_ =
        std::make_unique<trantor::EventLoopThreadPool>(threadNum_,
                                                       "DrogonIoLoop");
    std::vector<trantor::EventLoop *> ioLoops = ioLoopThreadPool_->getLoops();
    for (size_t i = 0; i < threadNum_; ++i)
    {
        ioLoops[i]->setIndex(i);
    }
    getLoop()->setIndex(threadNum_);

    // Create all listeners.
    listenerManagerPtr_->createListeners(
        [this](const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback) {
            onAsyncRequest(req, std::move(callback));
        },
        [this](const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               const WebSocketConnectionImplPtr &wsConnPtr) {
            onNewWebsockRequest(req, std::move(callback), wsConnPtr);
        },
        [this](const trantor::TcpConnectionPtr &conn) { onConnection(conn); },
        idleConnectionTimeout_,
        sslCertPath_,
        sslKeyPath_,
        sslConfCmds_,
        ioLoops,
        syncAdvices_,
        preSendingAdvices_);

    // A fast database client instance should be created in the main event
    // loop, so put the main loop into ioLoops.
    ioLoops.push_back(getLoop());
    dbClientManagerPtr_->createDbClients(ioLoops);
    redisClientManagerPtr_->createRedisClients(ioLoops);
    if (useSession_)
    {
        sessionManagerPtr_ =
            std::make_unique<SessionManager>(getLoop(),
                                             sessionTimeout_,
                                             sessionStartAdvices_,
                                             sessionDestroyAdvices_);
    }
    // now start running!!
    running_ = true;
    // Initialize plugins
    auto &pluginConfig = jsonConfig_["plugins"];
    const auto &runtumePluginConfig = jsonRuntimeConfig_["plugins"];
    if (!pluginConfig.isNull())
    {
        if (!runtumePluginConfig.isNull() && runtumePluginConfig.isArray())
        {
            for (const auto &plugin : runtumePluginConfig)
            {
                pluginConfig.append(plugin);
            }
        }
    }
    else
    {
        jsonConfig_["plugins"] = runtumePluginConfig;
    }
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
    routersInit_ = true;
    httpCtrlsRouterPtr_->init(ioLoops);
    httpSimpleCtrlsRouterPtr_->init(ioLoops);
    staticFileRouterPtr_->init(ioLoops);
    websockCtrlsRouterPtr_->init();
    getLoop()->queueInLoop([this]() {
        for (auto &adv : beginningAdvices_)
        {
            adv();
        }
        beginningAdvices_.clear();
        // Let listener event loops run when everything is ready.
        listenerManagerPtr_->startListening();
    });
    // start all loops
    // TODO: when should IOLoops start?
    // In before, IOLoops are started in `listenerManagerPtr_->startListening()`
    // It should be fine for them to start anywhere before `startListening()`.
    // However, we should consider other components.
    ioLoopThreadPool_->start();
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

    filesystem::path fsUploadPath(utils::toNativePath(uploadPath));
    if (!fsUploadPath.is_absolute())
    {
        filesystem::path fsRoot(utils::toNativePath(rootPath_));
        fsUploadPath = fsRoot / fsUploadPath;
    }
    uploadPath_ = utils::fromNativePath(fsUploadPath.native());
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
                jsessionid.setSameSite(sessionSameSite_);
                newResp->addCookie(std::move(jsessionid));
                sessionPtr->hasSet();
                callback(newResp);
                return;
            }
            else
            {
                auto jsessionid = Cookie("JSESSIONID", sessionPtr->sessionId());
                jsessionid.setPath("/");
                jsessionid.setSameSite(sessionSameSite_);
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
    if (!ioLoopThreadPool_)
    {
        LOG_WARN << "Please call getIOLoop() after drogon::app().run()";
        return nullptr;
    }
    auto n = ioLoopThreadPool_->size();
    if (id >= n)
    {
        LOG_TRACE << "Loop id (" << id << ") out of range [0-" << n << ").";
        id %= n;
        LOG_TRACE << "Rounded to : " << id;
    }
    return ioLoopThreadPool_->getLoop(id);
}

HttpAppFramework &HttpAppFramework::instance()
{
    return HttpAppFrameworkImpl::instance();
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
nosql::RedisClientPtr HttpAppFrameworkImpl::getRedisClient(
    const std::string &name)
{
    return redisClientManagerPtr_->getRedisClient(name);
}
nosql::RedisClientPtr HttpAppFrameworkImpl::getFastRedisClient(
    const std::string &name)
{
    return redisClientManagerPtr_->getFastRedisClient(name);
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
    const std::string &characterSet,
    double timeout,
    const bool autoBatch)
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
                                        characterSet,
                                        timeout,
                                        autoBatch);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::createRedisClient(
    const std::string &ip,
    unsigned short port,
    const std::string &name,
    const std::string &password,
    size_t connectionNum,
    bool isFast,
    double timeout,
    unsigned int db,
    const std::string &username)
{
    assert(!running_);
    redisClientManagerPtr_->createRedisClient(
        name, ip, port, username, password, connectionNum, isFast, timeout, db);
    return *this;
}
void HttpAppFrameworkImpl::quit()
{
    if (getLoop()->isRunning())
    {
        getLoop()->queueInLoop([this]() {
            // Release members in the reverse order of initialization
            listenerManagerPtr_->stopListening();
            listenerManagerPtr_.reset();
            websockCtrlsRouterPtr_.reset();
            staticFileRouterPtr_.reset();
            httpSimpleCtrlsRouterPtr_.reset();
            httpCtrlsRouterPtr_.reset();
            pluginsManagerPtr_.reset();
            redisClientManagerPtr_.reset();
            dbClientManagerPtr_.reset();
            running_ = false;
            getLoop()->quit();
            for (trantor::EventLoop *loop : ioLoopThreadPool_->getLoops())
            {
                loop->quit();
            }
            ioLoopThreadPool_->wait();
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
            thread404Pages.init(
                [this](HttpResponsePtr &resp, size_t /*index*/) {
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
HttpAppFramework &HttpAppFrameworkImpl::setDefaultHandler(
    DefaultHandler handler)
{
    staticFileRouterPtr_->setDefaultHandler(std::move(handler));
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setupFileLogger()
{
    if (!logPath_.empty() && !asyncFileLoggerPtr_)
    {
        // std::filesystem does not provide a method to check access
        // permissions, so keep existing code
        if (os_access(utils::toNativePath(logPath_).c_str(), R_OK | W_OK) != 0)
        {
            LOG_ERROR << "log file path not exist";
            abort();
        }
        else
        {
            std::string baseName = logfileBaseName_;
            if (baseName.empty())
            {
                baseName = "drogon";
            }
            asyncFileLoggerPtr_ = std::make_shared<trantor::AsyncFileLogger>();
            asyncFileLoggerPtr_->setFileName(baseName, ".log", logPath_);
            asyncFileLoggerPtr_->startLogging();
            asyncFileLoggerPtr_->setFileSizeLimit(logfileSize_);
            asyncFileLoggerPtr_->setMaxFiles(logfileMaxNum_);
            trantor::Logger::setOutputFunction(
                [loggerPtr = asyncFileLoggerPtr_](const char *msg,
                                                  const uint64_t len) {
                    loggerPtr->output(msg, len);
                },
                [loggerPtr = asyncFileLoggerPtr_]() { loggerPtr->flush(); });
        }
    }
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerCustomExtensionMime(
    const std::string &ext,
    const std::string &mime)
{
    drogon::registerCustomExtensionMime(ext, mime);
    return *this;
}
