/**
 *
 *  HttpAppFrameworkImpl.cc
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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <uuid.h>

using namespace drogon;
using namespace std::placeholders;

HttpAppFrameworkImpl::HttpAppFrameworkImpl()
    : _staticFileRouterPtr(new (StaticFileRouter)),
      _httpCtrlsRouterPtr(new HttpControllersRouter(*_staticFileRouterPtr,
                                                    _postRoutingAdvices,
                                                    _postRoutingObservers,
                                                    _preHandlingAdvices,
                                                    _preHandlingObservers,
                                                    _postHandlingAdvices)),
      _httpSimpleCtrlsRouterPtr(
          new HttpSimpleControllersRouter(*_httpCtrlsRouterPtr,
                                          _postRoutingAdvices,
                                          _postRoutingObservers,
                                          _preHandlingAdvices,
                                          _preHandlingObservers,
                                          _postHandlingAdvices)),
      _websockCtrlsRouterPtr(new WebsocketControllersRouter),
      _listenerManagerPtr(new ListenerManager),
      _pluginsManagerPtr(new PluginsManager),
      _dbClientManagerPtr(new orm::DbClientManager),
      _uploadPath(_rootPath + "uploads"),
      _connectionNum(0)
{
}
/// Make sure that the main event loop is initialized in the main thread.
drogon::InitBeforeMainFunction drogon::HttpAppFrameworkImpl::_initFirst([]() {
    HttpAppFrameworkImpl::instance().getLoop()->runInLoop([]() {
        LOG_TRACE << "Initialize the main event loop in the main thread";
    });
});

namespace drogon
{
std::string getVersion()
{
    return VERSION;
}
std::string getGitCommit()
{
    return VERSION_MD5;
}
}  // namespace drogon
static void godaemon(void)
{
    printf("Initializing daemon mode\n");

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

    return;
}
HttpAppFrameworkImpl::~HttpAppFrameworkImpl() noexcept
{
    // Destroy the following objects before _loop destruction
    _sharedLibManagerPtr.reset();
    _sessionManagerPtr.reset();
}
void HttpAppFrameworkImpl::setStaticFilesCacheTime(int cacheTime)
{
    _staticFileRouterPtr->setStaticFilesCacheTime(cacheTime);
}
int HttpAppFrameworkImpl::staticFilesCacheTime() const
{
    return _staticFileRouterPtr->staticFilesCacheTime();
}
void HttpAppFrameworkImpl::setGzipStatic(bool useGzipStatic)
{
    _staticFileRouterPtr->setGzipStatic(useGzipStatic);
}
void HttpAppFrameworkImpl::enableDynamicViewsLoading(
    const std::vector<std::string> &libPaths)
{
    assert(!_running);

    for (auto const &libpath : libPaths)
    {
        if (libpath[0] == '/' ||
            (libpath.length() >= 2 && libpath[0] == '.' && libpath[1] == '/') ||
            (libpath.length() >= 3 && libpath[0] == '.' && libpath[1] == '.' &&
             libpath[2] == '/') ||
            libpath == "." || libpath == "..")
        {
            _libFilePaths.push_back(libpath);
        }
        else
        {
            if (_rootPath[_rootPath.length() - 1] == '/')
                _libFilePaths.push_back(_rootPath + libpath);
            else
                _libFilePaths.push_back(_rootPath + "/" + libpath);
        }
    }
}
void HttpAppFrameworkImpl::setFileTypes(const std::vector<std::string> &types)
{
    _staticFileRouterPtr->setFileTypes(types);
}

void HttpAppFrameworkImpl::registerWebSocketController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<std::string> &filters)
{
    assert(!_running);
    _websockCtrlsRouterPtr->registerWebSocketController(pathName,
                                                        ctrlName,
                                                        filters);
}
void HttpAppFrameworkImpl::registerHttpSimpleController(
    const std::string &pathName,
    const std::string &ctrlName,
    const std::vector<internal::HttpConstraint> &filtersAndMethods)
{
    assert(!_running);
    _httpSimpleCtrlsRouterPtr->registerHttpSimpleController(pathName,
                                                            ctrlName,
                                                            filtersAndMethods);
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
    assert(!_running);
    _httpCtrlsRouterPtr->addHttpPath(
        pathPattern, binder, validMethods, filters, handlerName);
}
void HttpAppFrameworkImpl::setThreadNum(size_t threadNum)
{
    assert(threadNum >= 1);
    _threadNum = threadNum;
}
PluginBase *HttpAppFrameworkImpl::getPlugin(const std::string &name)
{
    return _pluginsManagerPtr->getPlugin(name);
}
void HttpAppFrameworkImpl::addListener(const std::string &ip,
                                       uint16_t port,
                                       bool useSSL,
                                       const std::string &certFile,
                                       const std::string &keyFile)
{
    assert(!_running);
    _listenerManagerPtr->addListener(ip, port, useSSL, certFile, keyFile);
}
void HttpAppFrameworkImpl::setMaxConnectionNum(size_t maxConnections)
{
    _maxConnectionNum = maxConnections;
}
void HttpAppFrameworkImpl::setMaxConnectionNumPerIP(size_t maxConnectionsPerIP)
{
    _maxConnectionNumPerIP = maxConnectionsPerIP;
}
void HttpAppFrameworkImpl::loadConfigFile(const std::string &fileName)
{
    ConfigLoader loader(fileName);
    loader.load();
    _jsonConfig = loader.jsonValue();
}
void HttpAppFrameworkImpl::setLogPath(const std::string &logPath,
                                      const std::string &logfileBaseName,
                                      size_t logfileSize)
{
    if (logPath == "")
        return;
    if (access(logPath.c_str(), 0) != 0)
    {
        std::cerr << "Log path dose not exist!\n";
        exit(1);
    }
    if (access(logPath.c_str(), R_OK | W_OK) != 0)
    {
        std::cerr << "Unable to access log path!\n";
        exit(1);
    }
    _logPath = logPath;
    _logfileBaseName = logfileBaseName;
    _logfileSize = logfileSize;
}
void HttpAppFrameworkImpl::setLogLevel(trantor::Logger::LogLevel level)
{
    trantor::Logger::setLogLevel(level);
}
void HttpAppFrameworkImpl::setSSLFiles(const std::string &certPath,
                                       const std::string &keyPath)
{
    _sslCertPath = certPath;
    _sslKeyPath = keyPath;
}

void HttpAppFrameworkImpl::run()
{
    //
    LOG_TRACE << "Start to run...";
    trantor::AsyncFileLogger asyncFileLogger;
    // Create dirs for cache files
    for (int i = 0; i < 256; i++)
    {
        char dirName[4];
        sprintf(dirName, "%02x", i);
        std::transform(dirName, dirName + 2, dirName, toupper);
        utils::createPath(getUploadPath() + "/tmp/" + dirName);
    }
    if (_runAsDaemon)
    {
        // go daemon!
        godaemon();
#ifdef __linux__
        getLoop()->resetTimerQueue();
#endif
        getLoop()->resetAfterFork();
    }
    // set relaunching
    if (_relaunchOnError)
    {
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
    }

    // set logger
    if (!_logPath.empty())
    {
        if (access(_logPath.c_str(), R_OK | W_OK) >= 0)
        {
            std::string baseName = _logfileBaseName;
            if (baseName == "")
            {
                baseName = "drogon";
            }
            asyncFileLogger.setFileName(baseName, ".log", _logPath);
            asyncFileLogger.startLogging();
            trantor::Logger::setOutputFunction(
                [&](const char *msg, const uint64_t len) {
                    asyncFileLogger.output(msg, len);
                },
                [&]() { asyncFileLogger.flush(); });
            asyncFileLogger.setFileSizeLimit(_logfileSize);
        }
        else
        {
            LOG_ERROR << "log file path not exist";
            abort();
        }
    }
    if (_relaunchOnError)
    {
        LOG_INFO << "Start child process";
    }
    // now start runing!!

    _running = true;

    if (!_libFilePaths.empty())
    {
        _sharedLibManagerPtr = std::unique_ptr<SharedLibManager>(
            new SharedLibManager(getLoop(), _libFilePaths));
    }
    // Create all listeners.
    auto ioLoops = _listenerManagerPtr->createListeners(
        std::bind(&HttpAppFrameworkImpl::onAsyncRequest, this, _1, _2),
        std::bind(&HttpAppFrameworkImpl::onNewWebsockRequest, this, _1, _2, _3),
        std::bind(&HttpAppFrameworkImpl::onConnection, this, _1),
        _idleConnectionTimeout,
        _sslCertPath,
        _sslKeyPath,
        _threadNum);
    // A fast database client instance should be created in the main event loop,
    // so put the main loop into ioLoops.
    ioLoops.push_back(getLoop());
    _dbClientManagerPtr->createDbClients(ioLoops);
    ioLoops.pop_back();
    _httpCtrlsRouterPtr->init(ioLoops);
    _httpSimpleCtrlsRouterPtr->init(ioLoops);
    _staticFileRouterPtr->init();
    _websockCtrlsRouterPtr->init();

    if (_useSession)
    {
        _sessionManagerPtr = std::unique_ptr<SessionManager>(
            new SessionManager(getLoop(), _sessionTimeout));
    }

    // Initialize plugins
    const auto &pluginConfig = _jsonConfig["plugins"];
    if (!pluginConfig.isNull())
    {
        _pluginsManagerPtr->initializeAllPlugins(pluginConfig,
                                                 [](PluginBase *plugin) {
                                                     // TODO: new plugin
                                                 });
    }

    // Let listener event loops run when everything is ready.
    _listenerManagerPtr->startListening();
    getLoop()->loop();
}

void HttpAppFrameworkImpl::onConnection(const trantor::TcpConnectionPtr &conn)
{
    static std::mutex mtx;
    LOG_TRACE << "connect!!!" << _maxConnectionNum
              << " num=" << _connectionNum.load();
    if (conn->connected())
    {
        if (_connectionNum.fetch_add(1) >= _maxConnectionNum)
        {
            LOG_ERROR << "too much connections!force close!";
            conn->forceClose();
            return;
        }
        else if (_maxConnectionNumPerIP > 0)
        {
            {
                std::lock_guard<std::mutex> lock(mtx);
                auto iter = _connectionsNumMap.find(conn->peerAddr().toIp());
                if (iter == _connectionsNumMap.end())
                {
                    _connectionsNumMap[conn->peerAddr().toIp()] = 1;
                }
                else if (iter->second++ > _maxConnectionNumPerIP)
                {
                    conn->getLoop()->queueInLoop(
                        [conn]() { conn->forceClose(); });
                    return;
                }
            }
        }
        for (auto &advice : _newConnectionAdvices)
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
        _connectionNum--;
        if (_maxConnectionNumPerIP > 0)
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto iter = _connectionsNumMap.find(conn->peerAddr().toIp());
            if (iter != _connectionsNumMap.end())
            {
                iter->second--;
                if (iter->second <= 0)
                {
                    _connectionsNumMap.erase(iter);
                }
            }
        }
    }
}

void HttpAppFrameworkImpl::setUploadPath(const std::string &uploadPath)
{
    assert(!uploadPath.empty());
    if (uploadPath[0] == '/' ||
        (uploadPath.length() >= 2 && uploadPath[0] == '.' &&
         uploadPath[1] == '/') ||
        (uploadPath.length() >= 3 && uploadPath[0] == '.' &&
         uploadPath[1] == '.' && uploadPath[2] == '/') ||
        uploadPath == "." || uploadPath == "..")
    {
        _uploadPath = uploadPath;
    }
    else
    {
        if (_rootPath[_rootPath.length() - 1] == '/')
            _uploadPath = _rootPath + uploadPath;
        else
            _uploadPath = _rootPath + "/" + uploadPath;
    }
}
void HttpAppFrameworkImpl::onNewWebsockRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const WebSocketConnectionImplPtr &wsConnPtr)
{
    _websockCtrlsRouterPtr->route(req, std::move(callback), wsConnPtr);
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpAppFrameworkImpl::getHandlersInfo() const
{
    auto ret = _httpSimpleCtrlsRouterPtr->getHandlersInfo();
    auto v = _httpCtrlsRouterPtr->getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    v = _websockCtrlsRouterPtr->getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    return ret;
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
        resp->addHeader("ALLOW", "GET,HEAD,POST,PUT,DELETE,OPTIONS");
        resp->setExpiredTime(0);
        callback(resp);
        return;
    }

    std::string sessionId = req->getCookie("JSESSIONID");
    bool needSetJsessionid = false;
    if (_useSession)
    {
        if (sessionId.empty())
        {
            sessionId = utils::getUuid().c_str();
            needSetJsessionid = true;
        }
        req->setSession(_sessionManagerPtr->getSession(sessionId));
    }

    // Route to controller
    if (!_preRoutingObservers.empty())
    {
        for (auto &observer : _preRoutingObservers)
        {
            observer(req);
        }
    }
    if (_preRoutingAdvices.empty())
    {
        _httpSimpleCtrlsRouterPtr->route(req,
                                         std::move(callback),
                                         needSetJsessionid,
                                         std::move(sessionId));
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
        doAdvicesChain(
            _preRoutingAdvices,
            0,
            req,
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                [callbackPtr, needSetJsessionid, sessionIdPtr](
                    const HttpResponsePtr &resp) {
                    if (!needSetJsessionid ||
                        resp->statusCode() == k404NotFound)
                        (*callbackPtr)(resp);
                    else
                    {
                        resp->addCookie("JSESSIONID", *sessionIdPtr);
                        (*callbackPtr)(resp);
                    }
                }),
            [this, callbackPtr, req, needSetJsessionid, sessionIdPtr]() {
                _httpSimpleCtrlsRouterPtr->route(req,
                                                 std::move(*callbackPtr),
                                                 needSetJsessionid,
                                                 std::move(*sessionIdPtr));
            });
    }
}

trantor::EventLoop *HttpAppFrameworkImpl::getLoop() const
{
    static trantor::EventLoop loop;
    return &loop;
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
    const std::string &hostString)
{
    forward(std::dynamic_pointer_cast<HttpRequestImpl>(req),
            std::move(callback),
            hostString);
}
void HttpAppFrameworkImpl::forward(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &hostString)
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
        clientPtr->sendRequest(
            req,
            [callback = std::move(callback)](ReqResult result,
                                             const HttpResponsePtr &resp) {
                if (result == ReqResult::Ok)
                {
                    resp->removeHeader("server");
                    resp->removeHeader("date");
                    resp->removeHeader("content-length");
                    resp->removeHeader("transfer-encoding");
                    callback(resp);
                }
                else
                {
                    callback(HttpResponse::newNotFoundResponse());
                }
            });
    }
}

orm::DbClientPtr HttpAppFrameworkImpl::getDbClient(const std::string &name)
{
    return _dbClientManagerPtr->getDbClient(name);
}
orm::DbClientPtr HttpAppFrameworkImpl::getFastDbClient(const std::string &name)
{
    return _dbClientManagerPtr->getFastDbClient(name);
}
void HttpAppFrameworkImpl::createDbClient(const std::string &dbType,
                                          const std::string &host,
                                          const u_short port,
                                          const std::string &databaseName,
                                          const std::string &userName,
                                          const std::string &password,
                                          const size_t connectionNum,
                                          const std::string &filename,
                                          const std::string &name,
                                          const bool isFast)
{
    assert(!_running);
    _dbClientManagerPtr->createDbClient(dbType,
                                        host,
                                        port,
                                        databaseName,
                                        userName,
                                        password,
                                        connectionNum,
                                        filename,
                                        name,
                                        isFast);
}