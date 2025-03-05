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
#include "HttpConnectionLimit.h"
#include "HttpControllersRouter.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpServer.h"
#include "HttpUtils.h"
#include "ListenerManager.h"
#include "PluginsManager.h"
#include "RedisClientManager.h"
#include "SessionManager.h"
#include "SharedLibManager.h"
#include "StaticFileRouter.h"

#include <iostream>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <filesystem>

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

#ifdef DROGON_SPDLOG_SUPPORT
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
// Damn antedeluvian M$ macros
#undef min
#undef max
#endif
#endif  // DROGON_SPDLOG_SUPPORT

using namespace drogon;
using namespace std::placeholders;

HttpAppFrameworkImpl::HttpAppFrameworkImpl()
    : listenerManagerPtr_(new ListenerManager),
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

HttpResponsePtr defaultErrorHandler(HttpStatusCode code, const HttpRequestPtr &)
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
    callback(handler(k500InternalServerError, req));
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
    StaticFileRouter::instance().setStaticFilesCacheTime(cacheTime);
    return *this;
}

int HttpAppFrameworkImpl::staticFilesCacheTime() const
{
    return StaticFileRouter::instance().staticFilesCacheTime();
}

HttpAppFramework &HttpAppFrameworkImpl::setGzipStatic(bool useGzipStatic)
{
    StaticFileRouter::instance().setGzipStatic(useGzipStatic);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setBrStatic(bool useGzipStatic)
{
    StaticFileRouter::instance().setBrStatic(useGzipStatic);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setImplicitPageEnable(
    bool useImplicitPage)
{
    StaticFileRouter::instance().setImplicitPageEnable(useImplicitPage);
    return *this;
}

bool HttpAppFrameworkImpl::isImplicitPageEnabled() const
{
    return StaticFileRouter::instance().isImplicitPageEnabled();
}

HttpAppFramework &HttpAppFrameworkImpl::setImplicitPage(
    const std::string &implicitPageFile)
{
    StaticFileRouter::instance().setImplicitPage(implicitPageFile);
    return *this;
}

const std::string &HttpAppFrameworkImpl::getImplicitPage() const
{
    return StaticFileRouter::instance().getImplicitPage();
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
    StaticFileRouter::instance().setFileTypes(types);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!routersInit_);
    HttpControllersRouter::instance().registerWebSocketController(pathName,
                                                                  ctrlName,
                                                                  constraints);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerWebSocketControllerRegex(
    const std::string &regExp,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!routersInit_);
    HttpControllersRouter::instance().registerWebSocketControllerRegex(
        regExp, ctrlName, constraints);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerHttpSimpleController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &constraints)
{
    assert(!routersInit_);
    HttpControllersRouter::instance().registerHttpSimpleController(pathName,
                                                                   ctrlName,
                                                                   constraints);
    return *this;
}

void HttpAppFrameworkImpl::registerHttpController(
    const std::string &pathPattern,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &middlewareNames,
    const std::string &handlerName)
{
    assert(!pathPattern.empty());
    assert(binder);
    assert(!routersInit_);
    HttpControllersRouter::instance().addHttpPath(
        pathPattern, binder, validMethods, middlewareNames, handlerName);
}

void HttpAppFrameworkImpl::registerHttpControllerViaRegex(
    const std::string &regExp,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &middlewareNames,
    const std::string &handlerName)
{
    assert(!regExp.empty());
    assert(binder);
    assert(!routersInit_);
    HttpControllersRouter::instance().addHttpRegex(
        regExp, binder, validMethods, middlewareNames, handlerName);
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
    HttpConnectionLimit::instance().setMaxConnectionNum(maxConnections);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setMaxConnectionNumPerIP(
    size_t maxConnectionsPerIP)
{
    HttpConnectionLimit::instance().setMaxConnectionNumPerIP(
        maxConnectionsPerIP);
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
    size_t maxFiles,
    bool useSpdlog)
{
#ifdef DROGON_SPDLOG_SUPPORT
    logWithSpdlog_ = trantor::Logger::hasSpdLogSupport() && useSpdlog;
#endif
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

HttpAppFramework &HttpAppFrameworkImpl::reloadSSLFiles()
{
    listenerManagerPtr_->reloadSSLFiles();
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
    listenerManagerPtr_->createListeners(sslCertPath_,
                                         sslKeyPath_,
                                         sslConfCmds_,
                                         ioLoops);

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
                                             sessionDestroyAdvices_,
                                             sessionIdGeneratorCallback_);
    }
    // now start running!!
    running_ = true;
    // Initialize plugins
    auto &pluginConfig = jsonConfig_["plugins"];
    const auto &runtimePluginConfig = jsonRuntimeConfig_["plugins"];
    if (!pluginConfig.isNull())
    {
        if (!runtimePluginConfig.isNull() && runtimePluginConfig.isArray())
        {
            for (const auto &plugin : runtimePluginConfig)
            {
                pluginConfig.append(plugin);
            }
        }
    }
    else
    {
        jsonConfig_["plugins"] = runtimePluginConfig;
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
    HttpControllersRouter::instance().init(ioLoops);
    StaticFileRouter::instance().init(ioLoops);
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

HttpAppFramework &HttpAppFrameworkImpl::setUploadPath(
    const std::string &uploadPath)
{
    assert(!uploadPath.empty());

    std::filesystem::path fsUploadPath(utils::toNativePath(uploadPath));
    if (!fsUploadPath.is_absolute())
    {
        std::filesystem::path fsRoot(utils::toNativePath(rootPath_));
        fsUploadPath = fsRoot / fsUploadPath;
    }
    uploadPath_ = utils::fromNativePath(fsUploadPath.native());
    return *this;
}

void HttpAppFrameworkImpl::findSessionForRequest(const HttpRequestImplPtr &req)
{
    if (useSession_)
    {
        std::string sessionId = req->getCookie(sessionCookieKey_);
        bool needSetSessionid = false;
        if (sessionId.empty())
        {
            sessionId = sessionIdGeneratorCallback_();
            needSetSessionid = true;
        }
        req->setSession(
            sessionManagerPtr_->getSession(sessionId, needSetSessionid));
    }
}

std::vector<HttpHandlerInfo> HttpAppFrameworkImpl::getHandlersInfo() const
{
    return HttpControllersRouter::instance().getHandlersInfo();
}

HttpResponsePtr HttpAppFrameworkImpl::handleSessionForResponse(
    const HttpRequestImplPtr &req,
    const HttpResponsePtr &resp)
{
    if (useSession_)
    {
        auto &sessionPtr = req->getSession();
        if (!sessionPtr)
        {
            return resp;
        }
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
                auto sessionid =
                    Cookie(sessionCookieKey_, sessionPtr->sessionId());
                sessionid.setPath("/");
                sessionid.setSameSite(sessionSameSite_);
                if (sessionMaxAge_ >= 0)
                    sessionid.setMaxAge(sessionMaxAge_);
                newResp->addCookie(std::move(sessionid));
                sessionPtr->hasSet();

                return newResp;
            }
            else
            {
                auto sessionid =
                    Cookie(sessionCookieKey_, sessionPtr->sessionId());
                sessionid.setPath("/");
                sessionid.setSameSite(sessionSameSite_);
                if (sessionMaxAge_ >= 0)
                    sessionid.setMaxAge(sessionMaxAge_);
                resp->addCookie(std::move(sessionid));
                sessionPtr->hasSet();

                return resp;
            }
        }
        else if (resp->version() != req->version())
        {
            auto newResp = std::make_shared<HttpResponseImpl>(
                *static_cast<HttpResponseImpl *>(resp.get()));
            newResp->setVersion(req->version());
            newResp->setExpiredTime(-1);  // make it temporary

            return newResp;
        }
        else
        {
            return resp;
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

            return newResp;
        }
        else
        {
            return resp;
        }
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
        HttpInternalForwardHelper::forward(req, std::move(callback));
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
            [callback = std::move(callback), req](ReqResult result,
                                                  const HttpResponsePtr &resp) {
                if (result == ReqResult::Ok)
                {
                    resp->setPassThrough(true);
                    callback(resp);
                }
                else
                {
                    callback(HttpResponse::newNotFoundResponse(req));
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

// deprecated
HttpAppFramework &HttpAppFrameworkImpl::createDbClient(
    const std::string &dbType,
    const std::string &host,
    unsigned short port,
    const std::string &databaseName,
    const std::string &userName,
    const std::string &password,
    size_t connectionNum,
    const std::string &filename,
    const std::string &name,
    bool isFast,
    const std::string &characterSet,
    double timeout,
    bool autoBatch)
{
    assert(!running_);
    addDbClient(dbType,
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
                autoBatch,
                {});
    return *this;
}

void HttpAppFrameworkImpl::addDbClient(
    const std::string &dbType,
    const std::string &host,
    unsigned short port,
    const std::string &databaseName,
    const std::string &userName,
    const std::string &password,
    size_t connectionNum,
    const std::string &filename,
    const std::string &name,
    bool isFast,
    const std::string &characterSet,
    double timeout,
    bool autoBatch,
    std::unordered_map<std::string, std::string> options)
{
    if (dbType == "postgresql" || dbType == "postgres")
    {
        addDbClient(orm::PostgresConfig{host,
                                        port,
                                        databaseName,
                                        userName,
                                        password,
                                        connectionNum,
                                        name,
                                        isFast,
                                        characterSet,
                                        timeout,
                                        autoBatch,
                                        std::move(options)});
    }
    else if (dbType == "mysql")
    {
        addDbClient(orm::MysqlConfig{host,
                                     port,
                                     databaseName,
                                     userName,
                                     password,
                                     connectionNum,
                                     name,
                                     isFast,
                                     characterSet,
                                     timeout});
    }
    else if (dbType == "sqlite3")
    {
        addDbClient(orm::Sqlite3Config{connectionNum, filename, name, timeout});
    }
    else
    {
        LOG_ERROR << "Unsupported database type: " << dbType
                  << ", should be one of (postgresql, mysql, sqlite3)";
    }
}

HttpAppFramework &HttpAppFrameworkImpl::addDbClient(const orm::DbConfig &config)
{
    assert(!running_);
    dbClientManagerPtr_->addDbClient(config);
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
    if (getLoop()->isRunning() && running_.exchange(false))
    {
        getLoop()->queueInLoop([this]() {
            // Release members in the reverse order of initialization
            listenerManagerPtr_->stopListening();
            listenerManagerPtr_.reset();
            StaticFileRouter::instance().reset();
            HttpControllersRouter::instance().reset();
            pluginsManagerPtr_.reset();
            redisClientManagerPtr_.reset();
            dbClientManagerPtr_.reset();
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
    StaticFileRouter::instance().setStaticFileHeaders(headers);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::addALocation(
    const std::string &uriPrefix,
    const std::string &defaultContentType,
    const std::string &alias,
    bool isCaseSensitive,
    bool allowAll,
    bool isRecursive,
    const std::vector<std::string> &middlewareNames)
{
    StaticFileRouter::instance().addALocation(uriPrefix,
                                              defaultContentType,
                                              alias,
                                              isCaseSensitive,
                                              allowAll,
                                              isRecursive,
                                              middlewareNames);
    return *this;
}

bool HttpAppFrameworkImpl::areAllDbClientsAvailable() const noexcept
{
    return dbClientManagerPtr_->areAllDbClientsAvailable();
}

HttpAppFramework &HttpAppFrameworkImpl::setCustomErrorHandler(
    std::function<HttpResponsePtr(HttpStatusCode, const HttpRequestPtr &req)>
        &&resp_generator)
{
    customErrorHandler_ = std::move(resp_generator);
    usingCustomErrorHandler_ = true;
    return *this;
}

const std::function<HttpResponsePtr(HttpStatusCode,
                                    const HttpRequestPtr &req)> &
HttpAppFrameworkImpl::getCustomErrorHandler() const
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
    StaticFileRouter::instance().setDefaultHandler(std::move(handler));
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setupFileLogger()
{
#ifdef DROGON_SPDLOG_SUPPORT
    if (logWithSpdlog_)
    {
        // Do nothing if already initialized...
        if (!trantor::Logger::getSpdLogger())
        {
            trantor::Logger::enableSpdLog();
            // Get the new logger & replace its sinks with the ones of the
            // config
            auto logger = trantor::Logger::getSpdLogger();
            std::vector<spdlog::sink_ptr> sinks;
            if (!logPath_.empty())
            {
                // 1. check existence of folder or try to create it
                auto fsLogPath =
                    std::filesystem::path(utils::toNativePath(logPath_));
                std::error_code fsErr;
                if (!std::filesystem::create_directories(fsLogPath, fsErr) &&
                    fsErr)
                {
                    LOG_ERROR << "could not create log file path";
                    abort();
                }
                // 2. check if we have rights to create files in the folder
                if (os_access(fsLogPath.native().c_str(), W_OK) != 0)
                {
                    LOG_ERROR << "cannot create files in log folder";
                    abort();
                }
                std::filesystem::path baseName(logfileBaseName_);
                if (baseName.empty())
                    baseName = "drogon.log";
                else
                    baseName.replace_extension(".log");
                auto sizeLimit = logfileSize_;
                if (sizeLimit == 0)  // 0 is not allowed by this sink
                    sizeLimit = std::numeric_limits<std::size_t>::max();
                sinks.push_back(
                    std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                        (fsLogPath / baseName).string(),
                        sizeLimit,
                        // spdlog limitation
                        std::min(logfileMaxNum_, std::size_t(20000)),
                        false));
            }
            else
                sinks.push_back(
                    std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
#if defined(_WIN32) && defined(_DEBUG)
            // On Windows with debug, it may be interesting to have the logs
            // directly in the Visual Studio / WinDbg console
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
            // Note: the new sinks won't use the format pattern set on the
            // logger, and there is currently not way to retrieve it.
            // So, set the same pattern as the one set on the logger in
            // trantor::Logger::getDefaultSpdLogger()
            for (auto &sink : sinks)
                sink->set_pattern("%Y%m%d %T.%f %6t %^%=8l%$ [%!] %v - %s:%#");
            logger->sinks() = sinks;
        }
        return *this;
    }
#endif  // DROGON_SPDLOG_SUPPORT
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

int64_t HttpAppFrameworkImpl::getConnectionCount() const
{
    return HttpConnectionLimit::instance().getConnectionNum();
}

HttpAppFramework &HttpAppFrameworkImpl::enableRequestStream(bool enable)
{
    enableRequestStream_ = enable;
    return *this;
}

bool HttpAppFrameworkImpl::isRequestStreamEnabled() const
{
    return enableRequestStream_;
}

// AOP registration methods

HttpAppFramework &HttpAppFrameworkImpl::registerNewConnectionAdvice(
    const std::function<bool(const trantor::InetAddress &,
                             const trantor::InetAddress &)> &advice)
{
    AopAdvice::instance().registerNewConnectionAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerHttpResponseCreationAdvice(
    const std::function<void(const HttpResponsePtr &)> &advice)
{
    // Is this callback really an AOP?
    // Maybe we should store them in HttpResponseImpl class as static member
    AopAdvice::instance().registerHttpResponseCreationAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerSyncAdvice(
    const std::function<HttpResponsePtr(const HttpRequestPtr &)> &advice)

{
    AopAdvice::instance().registerSyncAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPreRoutingAdvice(
    const std::function<void(const HttpRequestPtr &)> &advice)
{
    AopAdvice::instance().registerPreRoutingObserver(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPreRoutingAdvice(
    const std::function<void(const HttpRequestPtr &,
                             AdviceCallback &&,
                             AdviceChainCallback &&)> &advice)
{
    AopAdvice::instance().registerPreRoutingAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPostRoutingAdvice(
    const std::function<void(const HttpRequestPtr &)> &advice)
{
    AopAdvice::instance().registerPostRoutingObserver(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPostRoutingAdvice(
    const std::function<void(const HttpRequestPtr &,
                             AdviceCallback &&,
                             AdviceChainCallback &&)> &advice)
{
    AopAdvice::instance().registerPostRoutingAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPreHandlingAdvice(
    const std::function<void(const HttpRequestPtr &)> &advice)
{
    AopAdvice::instance().registerPreHandlingObserver(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPreHandlingAdvice(
    const std::function<void(const HttpRequestPtr &,
                             AdviceCallback &&,
                             AdviceChainCallback &&)> &advice)
{
    AopAdvice::instance().registerPreHandlingAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPostHandlingAdvice(
    const std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>
        &advice)
{
    AopAdvice::instance().registerPostHandlingAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::registerPreSendingAdvice(
    const std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>
        &advice)
{
    AopAdvice::instance().registerPreSendingAdvice(advice);
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setBeforeListenSockOptCallback(
    std::function<void(int)> cb)
{
    listenerManagerPtr_->setBeforeListenSockOptCallback(std::move(cb));
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setAfterAcceptSockOptCallback(
    std::function<void(int)> cb)
{
    listenerManagerPtr_->setAfterAcceptSockOptCallback(std::move(cb));
    return *this;
}

HttpAppFramework &HttpAppFrameworkImpl::setConnectionCallback(
    std::function<void(const trantor::TcpConnectionPtr &)> cb)
{
    listenerManagerPtr_->setConnectionCallback(std::move(cb));
    return *this;
}
