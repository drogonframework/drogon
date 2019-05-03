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
#include "ConfigLoader.h"
#include "HttpServer.h"
#include "AOPAdvice.h"
#if USE_ORM
#include "../../orm_lib/src/DbClientLockFree.h"
#endif
#include <drogon/HttpTypes.h>
#include <drogon/utils/Utilities.h>
#include <drogon/DrClassMap.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/CacheMap.h>
#include <drogon/Session.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <json/json.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <uuid.h>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

using namespace drogon;
using namespace std::placeholders;

///Make sure that the main event loop is initialized in the main thread.
drogon::InitBeforeMainFunction drogon::HttpAppFrameworkImpl::_initFirst([]() {
    HttpAppFrameworkImpl::instance().getLoop()->runInLoop([]() {
        LOG_TRACE << "Initialize the main event loop in the main thread";
    });
});
namespace drogon
{

class DrogonFileLocker : public trantor::NonCopyable
{
public:
    DrogonFileLocker()
    {
        _fd = open("/tmp/drogon.lock", O_TRUNC | O_CREAT, 0755);
        flock(_fd, LOCK_EX);
    }
    ~DrogonFileLocker()
    {
        close(_fd);
    }

private:
    int _fd = 0;
};

} // namespace drogon
static void godaemon(void)
{
    printf("Initializing daemon mode\n");

    if (getppid() != 1)
    {
        pid_t pid;
        pid = fork();
        if (pid > 0)
            exit(0); // parent
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

void HttpAppFrameworkImpl::enableDynamicViewsLoading(const std::vector<std::string> &libPaths)
{
    assert(!_running);

    for (auto const &libpath : libPaths)
    {
        if (libpath[0] == '/' ||
            (libpath.length() >= 2 && libpath[0] == '.' && libpath[1] == '/') ||
            (libpath.length() >= 3 && libpath[0] == '.' && libpath[1] == '.' && libpath[2] == '/') ||
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
    for (auto const &type : types)
    {
        _fileTypeSet.insert(type);
    }
}

void HttpAppFrameworkImpl::registerWebSocketController(const std::string &pathName,
                                                       const std::string &ctrlName,
                                                       const std::vector<std::string> &filters)
{
    assert(!_running);
    _websockCtrlsRouter.registerWebSocketController(pathName, ctrlName, filters);
}
void HttpAppFrameworkImpl::registerHttpSimpleController(const std::string &pathName,
                                                        const std::string &ctrlName,
                                                        const std::vector<any> &filtersAndMethods)
{
    assert(!_running);
    _httpSimpleCtrlsRouter.registerHttpSimpleController(pathName, ctrlName, filtersAndMethods);
}

void HttpAppFrameworkImpl::registerHttpController(const std::string &pathPattern,
                                                  const internal::HttpBinderBasePtr &binder,
                                                  const std::vector<HttpMethod> &validMethods,
                                                  const std::vector<std::string> &filters,
                                                  const std::string &handlerName)
{
    assert(!pathPattern.empty());
    assert(binder);
    assert(!_running);
    _httpCtrlsRouter.addHttpPath(pathPattern, binder, validMethods, filters, handlerName);
}
void HttpAppFrameworkImpl::setThreadNum(size_t threadNum)
{
    assert(threadNum >= 1);
    _threadNum = threadNum;
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
void HttpAppFrameworkImpl::addListener(const std::string &ip,
                                       uint16_t port,
                                       bool useSSL,
                                       const std::string &certFile,
                                       const std::string &keyFile)
{
    assert(!_running);

#ifndef USE_OPENSSL
    if (useSSL)
    {
        LOG_ERROR << "Can't use SSL without OpenSSL found in your system";
    }
#endif

    _listeners.push_back(std::make_tuple(ip, port, useSSL, certFile, keyFile));
}

void HttpAppFrameworkImpl::run()
{
    //
    LOG_INFO << "Start to run...";
    trantor::AsyncFileLogger asyncFileLogger;

    if (_runAsDaemon)
    {
        //go daemon!
        godaemon();
#ifdef __linux__
        getLoop()->resetTimerQueue();
#endif
        getLoop()->resetAfterFork();
    }
    //set relaunching
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
                //child
                break;
            }
            waitpid(child_pid, &child_status, 0);
            sleep(1);
            LOG_INFO << "start new process";
        }
        getLoop()->resetAfterFork();
    }

    //set logger
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
            trantor::Logger::setOutputFunction([&](const char *msg, const uint64_t len) { asyncFileLogger.output(msg, len); }, [&]() { asyncFileLogger.flush(); });
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
    //now start runing!!

    _running = true;

    if (!_libFilePaths.empty())
    {
        _sharedLibManagerPtr = std::unique_ptr<SharedLibManager>(new SharedLibManager(getLoop(), _libFilePaths));
    }
    std::vector<std::shared_ptr<HttpServer>> servers;
    std::vector<std::shared_ptr<EventLoopThread>> loopThreads;

    std::vector<trantor::EventLoop *> ioLoops;

#ifdef __linux__
    for (size_t i = 0; i < _threadNum; i++)
    {
        LOG_TRACE << "thread num=" << _threadNum;
        auto loopThreadPtr = std::make_shared<EventLoopThread>("DrogonIoLoop");
        loopThreads.push_back(loopThreadPtr);
        ioLoops.push_back(loopThreadPtr->getLoop());
        for (auto const &listener : _listeners)
        {
            auto ip = std::get<0>(listener);
            bool isIpv6 = ip.find(":") == std::string::npos ? false : true;
            std::shared_ptr<HttpServer> serverPtr;
            if (i == 0)
            {
                DrogonFileLocker lock;
                // Check whether the port is in use.
                TcpServer server(getLoop(),
                                 InetAddress(ip, std::get<1>(listener), isIpv6),
                                 "drogonPortTest",
                                 true,
                                 false);
                serverPtr = std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                         InetAddress(ip, std::get<1>(listener), isIpv6),
                                                         "drogon");
            }
            else
            {
                serverPtr = std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                         InetAddress(ip, std::get<1>(listener), isIpv6),
                                                         "drogon");
            }

            if (std::get<2>(listener))
            {
#ifdef USE_OPENSSL
                auto cert = std::get<3>(listener);
                auto key = std::get<4>(listener);
                if (cert == "")
                    cert = _sslCertPath;
                if (key == "")
                    key = _sslKeyPath;
                if (cert == "" || key == "")
                {
                    std::cerr << "You can't use https without cert file or key file" << std::endl;
                    exit(1);
                }
                serverPtr->enableSSL(cert, key);
#endif
            }
            serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest, this, _1, _2));
            serverPtr->setNewWebsocketCallback(std::bind(&HttpAppFrameworkImpl::onNewWebsockRequest, this, _1, _2, _3));
            serverPtr->setConnectionCallback(std::bind(&HttpAppFrameworkImpl::onConnection, this, _1));
            serverPtr->kickoffIdleConnections(_idleConnectionTimeout);
            serverPtr->start();
            servers.push_back(serverPtr);
        }
    }
#else
    for (auto const &listener : _listeners)
    {
        LOG_TRACE << "thread num=" << _threadNum;
        auto loopThreadPtr = std::make_shared<EventLoopThread>("DrogonListeningLoop");
        loopThreads.push_back(loopThreadPtr);
        auto ip = std::get<0>(listener);
        bool isIpv6 = ip.find(":") == std::string::npos ? false : true;
        auto serverPtr = std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                      InetAddress(ip, std::get<1>(listener), isIpv6),
                                                      "drogon");
        if (std::get<2>(listener))
        {
#ifdef USE_OPENSSL
            auto cert = std::get<3>(listener);
            auto key = std::get<4>(listener);
            if (cert == "")
                cert = _sslCertPath;
            if (key == "")
                key = _sslKeyPath;
            if (cert == "" || key == "")
            {
                std::cerr << "You can't use https without cert file or key file" << std::endl;
                exit(1);
            }
            serverPtr->enableSSL(cert, key);
#endif
        }
        serverPtr->setIoLoopNum(_threadNum);
        serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest, this, _1, _2));
        serverPtr->setNewWebsocketCallback(std::bind(&HttpAppFrameworkImpl::onNewWebsockRequest, this, _1, _2, _3));
        serverPtr->setConnectionCallback(std::bind(&HttpAppFrameworkImpl::onConnection, this, _1));
        serverPtr->kickoffIdleConnections(_idleConnectionTimeout);
        serverPtr->start();
        auto serverIoLoops = serverPtr->getIoLoops();
        for (auto serverIoLoop : serverIoLoops)
        {
            ioLoops.push_back(serverIoLoop);
        }
        servers.push_back(serverPtr);
    }
#endif

#if USE_ORM
    // A fast database client instance should be created in the main event loop, so put the main loop into ioLoops.
    ioLoops.push_back(getLoop());
    createDbClients(ioLoops);
    ioLoops.pop_back();
#endif
    _httpCtrlsRouter.init(ioLoops);
    _httpSimpleCtrlsRouter.init(ioLoops);
    _websockCtrlsRouter.init();

    if (_useSession)
    {
        if (_sessionTimeout > 0)
        {
            size_t wheelNum = 1;
            size_t bucketNum = 0;
            if (_sessionTimeout < 500)
            {
                bucketNum = _sessionTimeout + 1;
            }
            else
            {
                auto tmpTimeout = _sessionTimeout;
                bucketNum = 100;
                while (tmpTimeout > 100)
                {
                    wheelNum++;
                    tmpTimeout = tmpTimeout / 100;
                }
            }
            _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(new CacheMap<std::string, SessionPtr>(getLoop(), 1.0, wheelNum, bucketNum));
        }
        else if (_sessionTimeout == 0)
        {
            _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(new CacheMap<std::string, SessionPtr>(getLoop(), 0, 0, 0));
        }
    }
    _responseCachingMap = std::unique_ptr<CacheMap<std::string, HttpResponsePtr>>(new CacheMap<std::string, HttpResponsePtr>(getLoop(), 1.0, 4, 50)); //Max timeout up to about 70 days;

    //Initialize plugins
    const auto &pluginConfig = _jsonConfig["plugins"];
    if (!pluginConfig.isNull())
    {
        _pluginsManager.initializeAllPlugins(pluginConfig,
                                             [](PluginBase *plugin) {
                                                 //TODO: new plugin
                                             });
    }

    // Let listener event loops run when everything is ready.
    for (auto &loopTh : loopThreads)
    {
        loopTh->run();
    }
    getLoop()->loop();
}
#if USE_ORM
void HttpAppFrameworkImpl::createDbClients(const std::vector<trantor::EventLoop *> &ioloops)
{
    assert(_dbClientsMap.empty());
    assert(_dbFastClientsMap.empty());
    for (auto &dbInfo : _dbInfos)
    {
        if (dbInfo._isFast)
        {
            for (auto *loop : ioloops)
            {
                if (dbInfo._dbType == drogon::orm::ClientType::Sqlite3)
                {
                    LOG_ERROR << "Sqlite3 don't support fast mode";
                    abort();
                }
                if (dbInfo._dbType == drogon::orm::ClientType::PostgreSQL || dbInfo._dbType == drogon::orm::ClientType::Mysql)
                {
                    _dbFastClientsMap[dbInfo._name][loop] = std::shared_ptr<drogon::orm::DbClient>(new drogon::orm::DbClientLockFree(dbInfo._connectionInfo, loop, dbInfo._dbType));
                }
            }
        }
        else
        {
            if (dbInfo._dbType == drogon::orm::ClientType::PostgreSQL)
            {
#if USE_POSTGRESQL
                _dbClientsMap[dbInfo._name] = drogon::orm::DbClient::newPgClient(dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
            else if (dbInfo._dbType == drogon::orm::ClientType::Mysql)
            {
#if USE_MYSQL
                _dbClientsMap[dbInfo._name] = drogon::orm::DbClient::newMysqlClient(dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
            else if (dbInfo._dbType == drogon::orm::ClientType::Sqlite3)
            {
#if USE_SQLITE3
                _dbClientsMap[dbInfo._name] = drogon::orm::DbClient::newSqlite3Client(dbInfo._connectionInfo, dbInfo._connectionNumber);
#endif
            }
        }
    }
}
#endif

void HttpAppFrameworkImpl::onConnection(const TcpConnectionPtr &conn)
{
    static std::mutex mtx;
    LOG_TRACE << "connect!!!" << _maxConnectionNum << " num=" << _connectionNum.load();
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
                    conn->getLoop()->queueInLoop([conn]() {
                        conn->forceClose();
                    });
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
#if CXX_STD >= 17
        if (!conn->getContext().has_value())
#else
        if (conn->getContext().empty())
#endif
        {
            //If the connection is connected to the SSL port and then disconnected before the SSL handshake.
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
        (uploadPath.length() >= 2 && uploadPath[0] == '.' && uploadPath[1] == '/') ||
        (uploadPath.length() >= 3 && uploadPath[0] == '.' && uploadPath[1] == '.' && uploadPath[2] == '/') ||
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
void HttpAppFrameworkImpl::onNewWebsockRequest(const HttpRequestImplPtr &req,
                                               std::function<void(const HttpResponsePtr &)> &&callback,
                                               const WebSocketConnectionImplPtr &wsConnPtr)
{
    _websockCtrlsRouter.route(req, std::move(callback), wsConnPtr);
}

std::vector<std::tuple<std::string, HttpMethod, std::string>> HttpAppFrameworkImpl::getHandlersInfo() const
{
    auto ret = _httpSimpleCtrlsRouter.getHandlersInfo();
    auto v = _httpCtrlsRouter.getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    v = _websockCtrlsRouter.getHandlersInfo();
    ret.insert(ret.end(), v.begin(), v.end());
    return ret;
}

void HttpAppFrameworkImpl::onAsyncRequest(const HttpRequestImplPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
{
    LOG_TRACE << "new request:" << req->peerAddr().toIpPort() << "->" << req->localAddr().toIpPort();
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
    // LOG_TRACE << "query: " << req->query() ;

    std::string sessionId = req->getCookie("JSESSIONID");
    bool needSetJsessionid = false;
    if (_useSession)
    {
        if (sessionId == "")
        {
            sessionId = utils::getUuid().c_str();
            needSetJsessionid = true;
            _sessionMapPtr->insert(sessionId, std::make_shared<Session>(), _sessionTimeout);
        }
        else
        {
            if (_sessionMapPtr->find(sessionId) == false)
            {
                _sessionMapPtr->insert(sessionId, std::make_shared<Session>(), _sessionTimeout);
            }
        }
        (std::dynamic_pointer_cast<HttpRequestImpl>(req))->setSession((*_sessionMapPtr)[sessionId]);
    }

    const std::string &path = req->path();
    auto pos = path.rfind(".");
    if (pos != std::string::npos)
    {
        std::string filetype = path.substr(pos + 1);
        transform(filetype.begin(), filetype.end(), filetype.begin(), tolower);
        if (_fileTypeSet.find(filetype) != _fileTypeSet.end())
        {
            //LOG_INFO << "file query!" << path;
            std::string filePath = _rootPath + path;
            if (filePath.find("/../") != std::string::npos)
            {
                //Downloading files from the parent folder is forbidden.
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k403Forbidden);
                callback(resp);
                return;
            }
            //find cached response
            HttpResponsePtr cachedResp;
            {
                std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
                if (_staticFilesCache.find(filePath) != _staticFilesCache.end())
                {
                    cachedResp = _staticFilesCache[filePath].lock();
                    if (!cachedResp)
                    {
                        _staticFilesCache.erase(filePath);
                    }
                }
            }

            //check last modified time,rfc2616-14.25
            //If-Modified-Since: Mon, 15 Oct 2018 06:26:33 GMT

            std::string timeStr;
            if (_enableLastModify)
            {
                if (cachedResp)
                {
                    if (std::dynamic_pointer_cast<HttpResponseImpl>(cachedResp)->getHeaderBy("last-modified") == req->getHeaderBy("if-modified-since"))
                    {
                        std::shared_ptr<HttpResponseImpl> resp = std::make_shared<HttpResponseImpl>();
                        resp->setStatusCode(k304NotModified);
                        if (needSetJsessionid)
                        {
                            resp->addCookie("JSESSIONID", sessionId);
                        }
                        callback(resp);
                        return;
                    }
                }
                else
                {
                    struct stat fileStat;
                    LOG_TRACE << "enabled LastModify";
                    if (stat(filePath.c_str(), &fileStat) >= 0)
                    {
                        LOG_TRACE << "last modify time:" << fileStat.st_mtime;
                        struct tm tm1;
                        gmtime_r(&fileStat.st_mtime, &tm1);
                        timeStr.resize(64);
                        auto len = strftime((char *)timeStr.data(), timeStr.size(), "%a, %d %b %Y %T GMT", &tm1);
                        timeStr.resize(len);
                        const std::string &modiStr = req->getHeaderBy("if-modified-since");
                        if (modiStr == timeStr && !modiStr.empty())
                        {
                            LOG_TRACE << "not Modified!";
                            std::shared_ptr<HttpResponseImpl> resp = std::make_shared<HttpResponseImpl>();
                            resp->setStatusCode(k304NotModified);
                            if (needSetJsessionid)
                            {
                                resp->addCookie("JSESSIONID", sessionId);
                            }
                            callback(resp);
                            return;
                        }
                    }
                }
            }

            if (cachedResp)
            {
                if (needSetJsessionid)
                {
                    //make a copy
                    auto newCachedResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(cachedResp));
                    newCachedResp->addCookie("JSESSIONID", sessionId);
                    newCachedResp->setExpiredTime(-1);
                    callback(newCachedResp);
                    return;
                }
                callback(cachedResp);
                return;
            }
            HttpResponsePtr resp;
            if (_gzipStaticFlag && req->getHeaderBy("accept-encoding").find("gzip") != std::string::npos)
            {
                //Find compressed file first.
                auto gzipFileName = filePath + ".gz";
                std::ifstream infile(gzipFileName, std::ifstream::binary);
                if (infile)
                {
                    resp = HttpResponse::newFileResponse(gzipFileName, "", drogon::getContentType(filePath));
                    resp->addHeader("Content-Encoding", "gzip");
                }
            }
            if (!resp)
                resp = HttpResponse::newFileResponse(filePath);
            if (resp->statusCode() != k404NotFound)
            {
                if (!timeStr.empty())
                {
                    resp->addHeader("Last-Modified", timeStr);
                    resp->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
                }
                //cache the response for 5 seconds by default
                if (_staticFilesCacheTime >= 0)
                {
                    resp->setExpiredTime(_staticFilesCacheTime);
                    _responseCachingMap->insert(filePath, resp, resp->expiredTime(), [=]() {
                        std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
                        _staticFilesCache.erase(filePath);
                    });
                    {
                        std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
                        _staticFilesCache[filePath] = resp;
                    }
                }

                if (needSetJsessionid)
                {
                    auto newCachedResp = resp;
                    if (resp->expiredTime() >= 0)
                    {
                        //make a copy
                        newCachedResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                        newCachedResp->setExpiredTime(-1);
                    }
                    newCachedResp->addCookie("JSESSIONID", sessionId);
                    callback(newCachedResp);
                    return;
                }
            }
            callback(resp);
            return;
        }
    }

    //Route to controller
    if (!_preRoutingObservers.empty())
    {
        for (auto &observer : _preRoutingObservers)
        {
            observer(req);
        }
    }
    if (_preRoutingAdvices.empty())
    {
        _httpSimpleCtrlsRouter.route(req, std::move(callback), needSetJsessionid, std::move(sessionId));
    }
    else
    {
        auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
        auto sessionIdPtr = std::make_shared<std::string>(std::move(sessionId));
        doAdvicesChain(_preRoutingAdvices,
                       0,
                       req,
                       std::make_shared<std::function<void(const HttpResponsePtr &)>>([callbackPtr, needSetJsessionid, sessionIdPtr](const HttpResponsePtr &resp) {
                           if (!needSetJsessionid)
                               (*callbackPtr)(resp);
                           else
                           {
                               resp->addCookie("JSESSIONID", *sessionIdPtr);
                               (*callbackPtr)(resp);
                           }
                       }),
                       [this, callbackPtr, req, needSetJsessionid, sessionIdPtr]() {
                           _httpSimpleCtrlsRouter.route(req, std::move(*callbackPtr), needSetJsessionid, std::move(*sessionIdPtr));
                       });
    }
}

trantor::EventLoop *HttpAppFrameworkImpl::getLoop()
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

#if USE_ORM
orm::DbClientPtr HttpAppFrameworkImpl::getDbClient(const std::string &name)
{
    assert(_dbClientsMap.find(name) != _dbClientsMap.end());
    return _dbClientsMap[name];
}
orm::DbClientPtr HttpAppFrameworkImpl::getFastDbClient(const std::string &name)
{
    assert(_dbFastClientsMap[name].find(trantor::EventLoop::getEventLoopOfCurrentThread()) !=
           _dbFastClientsMap[name].end());
    return _dbFastClientsMap[name][trantor::EventLoop::getEventLoopOfCurrentThread()];
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
    auto connStr = utils::formattedString("host=%s port=%u dbname=%s user=%s", host.c_str(), port, databaseName.c_str(), userName.c_str());
    if (!password.empty())
    {
        connStr += " password=";
        connStr += password;
    }
    std::string type = dbType;
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    DbInfo info;
    info._connectionInfo = connStr;
    info._connectionNumber = connectionNum;
    info._isFast = isFast;
    info._name = name;

    if (type == "postgresql")
    {
#if USE_POSTGRESQL
        info._dbType = orm::ClientType::PostgreSQL;
        _dbInfos.push_back(info);
#else
        std::cout << "The PostgreSQL is not supported by drogon, please install the development library first." << std::endl;
        exit(1);
#endif
    }
    else if (type == "mysql")
    {
#if USE_MYSQL
        info._dbType = orm::ClientType::Mysql;
        _dbInfos.push_back(info);
#else
        std::cout << "The Mysql is not supported by drogon, please install the development library first." << std::endl;
        exit(1);
#endif
    }
    else if (type == "sqlite3")
    {
#if USE_SQLITE3
        std::string sqlite3ConnStr = "filename=" + filename;
        info._connectionInfo = sqlite3ConnStr;
        info._dbType = orm::ClientType::Sqlite3;
        _dbInfos.push_back(info);
#else
        std::cout << "The Sqlite3 is not supported by drogon, please install the development library first." << std::endl;
        exit(1);
#endif
    }
}
#endif
