/**
 *
 *  HttpAppFrameworkImpl.h
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

#pragma once

#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpClientImpl.h"
#include "SharedLibManager.h"
#include "WebSockectConnectionImpl.h"
#include "HttpControllersRouter.h"
#include "HttpSimpleControllersRouter.h"
#include "WebsocketControllersRouter.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpSimpleController.h>
#include <drogon/version.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThread.h>

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <regex>

namespace drogon
{
struct InitBeforeMainFunction
{
    InitBeforeMainFunction(const std::function<void()> &func)
    {
        func();
    }
};
class HttpAppFrameworkImpl : public HttpAppFramework
{
  public:
    HttpAppFrameworkImpl()
        : _httpSimpleCtrlsRouter(_httpCtrlsRouter),
          _uploadPath(_rootPath + "uploads"),
          _connectionNum(0)
    {
    }
    virtual const Json::Value &getCustomConfig() const override
    {
        return _customConfig;
    }
    virtual void addListener(const std::string &ip,
                             uint16_t port,
                             bool useSSL = false,
                             const std::string &certFile = "",
                             const std::string &keyFile = "") override;
    virtual void setThreadNum(size_t threadNum) override;
    virtual size_t getThreadNum() const override { return _threadNum; }
    virtual void setSSLFiles(const std::string &certPath,
                             const std::string &keyPath) override;
    virtual void run() override;
    virtual void registerWebSocketController(const std::string &pathName,
                                             const std::string &crtlName,
                                             const std::vector<std::string> &filters =
                                                 std::vector<std::string>()) override;
    virtual void registerHttpSimpleController(const std::string &pathName,
                                              const std::string &crtlName,
                                              const std::vector<any> &filtersAndMethods =
                                                  std::vector<any>()) override;
    virtual void enableSession(const size_t timeout = 0) override
    {
        _useSession = true;
        _sessionTimeout = timeout;
    }
    virtual void disableSession() override { _useSession = false; }
    virtual const std::string &getDocumentRoot() const override { return _rootPath; }
    virtual void setDocumentRoot(const std::string &rootPath) override { _rootPath = rootPath; }
    virtual const std::string &getUploadPath() const override { return _uploadPath; }
    virtual void setUploadPath(const std::string &uploadPath) override;
    virtual void setFileTypes(const std::vector<std::string> &types) override;
    virtual void enableDynamicViewsLoading(const std::vector<std::string> &libPaths) override;
    virtual void setMaxConnectionNum(size_t maxConnections) override;
    virtual void setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) override;
    virtual void loadConfigFile(const std::string &fileName) override;
    virtual void enableRunAsDaemon() override { _runAsDaemon = true; }
    virtual void enableRelaunchOnError() override { _relaunchOnError = true; }
    virtual void setLogPath(const std::string &logPath,
                            const std::string &logfileBaseName = "",
                            size_t logfileSize = 100000000) override;
    virtual void setLogLevel(trantor::Logger::LogLevel level) override;
    virtual void enableSendfile(bool sendFile) override { _useSendfile = sendFile; }
    virtual void enableGzip(bool useGzip) override { _useGzip = useGzip; }
    virtual bool useGzip() const override { return _useGzip; }
    virtual void setStaticFilesCacheTime(int cacheTime) override { _staticFilesCacheTime = cacheTime; }
    virtual int staticFilesCacheTime() const override { return _staticFilesCacheTime; }
    virtual void setIdleConnectionTimeout(size_t timeout) override { _idleConnectionTimeout = timeout; }
    virtual void setKeepaliveRequestsNumber(const size_t number) override { _keepaliveRequestsNumber = number; }
    virtual void setPipelineRequestsNumber(const size_t number) override { _pipelineRequestsNumber = number; }
    size_t keepaliveRequestsNumber() const { return _keepaliveRequestsNumber; }
    size_t pipelineRequestsNumber() const { return _pipelineRequestsNumber; }

    virtual ~HttpAppFrameworkImpl() noexcept
    {
        //Destroy the following objects before _loop destruction
        _sharedLibManagerPtr.reset();
        _sessionMapPtr.reset();
    }

    virtual bool isRunning() override
    {
        return _running;
    }

    virtual trantor::EventLoop *getLoop() override;

    virtual void quit() override
    {
        if (getLoop()->isRunning())
            getLoop()->quit();
    }

    virtual void setServerHeaderField(const std::string &server) override
    {
        assert(!_running);
        assert(server.find("\r\n") == std::string::npos);
        _serverHeader = "Server: " + server + "\r\n";
    }

    const std::string &getServerHeaderString() const
    {
        return _serverHeader;
    }

#if USE_ORM
    virtual orm::DbClientPtr getDbClient(const std::string &name = "default") override;
    virtual orm::DbClientPtr getFastDbClient(const std::string &name = "default") override;
    virtual void createDbClient(const std::string &dbType,
                                const std::string &host,
                                const u_short port,
                                const std::string &databaseName,
                                const std::string &userName,
                                const std::string &password,
                                const size_t connectionNum = 1,
                                const std::string &filename = "",
                                const std::string &name = "default",
                                const bool isFast = false) override;
#endif

    inline static HttpAppFrameworkImpl &instance()
    {
        static HttpAppFrameworkImpl _instance;
        return _instance;
    }
    bool useSendfile() { return _useSendfile; }

  private:
    virtual void registerHttpController(const std::string &pathPattern,
                                        const internal::HttpBinderBasePtr &binder,
                                        const std::vector<HttpMethod> &validMethods = std::vector<HttpMethod>(),
                                        const std::vector<std::string> &filters = std::vector<std::string>()) override;
    void onAsyncRequest(const HttpRequestImplPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void onNewWebsockRequest(const HttpRequestImplPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback,
                             const WebSocketConnectionPtr &wsConnPtr);
    void onWebsockMessage(const WebSocketConnectionPtr &wsConnPtr, std::string &&message);
    void onWebsockDisconnect(const WebSocketConnectionPtr &wsConnPtr);
    void onConnection(const TcpConnectionPtr &conn);
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &filters);

    //We use a uuid string as session id;
    //set _sessionTimeout=0 to make location session valid forever based on cookies;
    size_t _sessionTimeout = 0;
    size_t _idleConnectionTimeout = 60;
    bool _useSession = false;
    std::vector<std::tuple<std::string, uint16_t, bool, std::string, std::string>> _listeners;
    std::string _serverHeader = "Server: drogon/" + drogon::getVersion() + "\r\n";

    typedef std::shared_ptr<Session> SessionPtr;
    std::unique_ptr<CacheMap<std::string, SessionPtr>> _sessionMapPtr;
    std::unique_ptr<CacheMap<std::string, HttpResponsePtr>> _responseCachingMap;

    HttpControllersRouter _httpCtrlsRouter;
    HttpSimpleControllersRouter _httpSimpleCtrlsRouter;

    WebsocketControllersRouter _websockCtrlsRouter;

    bool _enableLastModify = true;
    std::set<std::string> _fileTypeSet = {"html", "js", "css", "xml", "xsl", "txt", "svg", "ttf",
                                          "otf", "woff2", "woff", "eot", "png", "jpg", "jpeg",
                                          "gif", "bmp", "ico", "icns"};
    std::string _rootPath = "./";
    std::string _uploadPath;
    std::atomic_bool _running;

    size_t _threadNum = 1;
    std::vector<std::string> _libFilePaths;

    std::unique_ptr<SharedLibManager> _sharedLibManagerPtr;

    std::string _sslCertPath;
    std::string _sslKeyPath;

    size_t _maxConnectionNumPerIP = 0;
    std::unordered_map<std::string, size_t> _connectionsNumMap;

    int64_t _maxConnectionNum = 100000;
    std::atomic<int64_t> _connectionNum;

    bool _runAsDaemon = false;
    bool _relaunchOnError = false;
    std::string _logPath = "";
    std::string _logfileBaseName = "";
    size_t _logfileSize = 100000000;
    size_t _keepaliveRequestsNumber = 0;
    size_t _pipelineRequestsNumber = 0;
    bool _useSendfile = true;
    bool _useGzip = true;
    int _staticFilesCacheTime = 5;
    std::unordered_map<std::string, std::weak_ptr<HttpResponse>> _staticFilesCache;
    std::mutex _staticFilesCacheMutex;
    Json::Value _customConfig;
#if USE_ORM
    std::map<std::string, orm::DbClientPtr> _dbClientsMap;
    struct DbInfo
    {
        std::string _name;
        std::string _connectionInfo;
        orm::ClientType _dbType;
        bool _isFast;
        size_t _connectionNumber;
    };
    std::vector<DbInfo> _dbInfos;
    std::map<std::string, std::map<trantor::EventLoop *, orm::DbClientPtr>> _dbFastClientsMap;
    void createDbClients(const std::vector<trantor::EventLoop *> &ioloops);
#endif
    static InitBeforeMainFunction _initFirst;
};

} // namespace drogon
