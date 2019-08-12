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

#include "DbClientManager.h"
#include "HttpClientImpl.h"
#include "HttpControllersRouter.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpSimpleControllersRouter.h"
#include "PluginsManager.h"
#include "ListenerManager.h"
#include "SharedLibManager.h"
#include "WebSocketConnectionImpl.h"
#include "WebsocketControllersRouter.h"
#include "StaticFileRouter.h"
#include "SessionManager.h"
#include <drogon/config.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpSimpleController.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThread.h>

#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

namespace drogon
{
struct InitBeforeMainFunction
{
    explicit InitBeforeMainFunction(const std::function<void()> &func)
    {
        func();
    }
};
class HttpAppFrameworkImpl : public HttpAppFramework
{
  public:
    HttpAppFrameworkImpl()
        : _httpCtrlsRouter(_staticFileRouter,
                           _postRoutingAdvices,
                           _postRoutingObservers,
                           _preHandlingAdvices,
                           _preHandlingObservers,
                           _postHandlingAdvices),
          _httpSimpleCtrlsRouter(_httpCtrlsRouter,
                                 _postRoutingAdvices,
                                 _postRoutingObservers,
                                 _preHandlingAdvices,
                                 _preHandlingObservers,
                                 _postHandlingAdvices),
          _uploadPath(_rootPath + "uploads"),
          _connectionNum(0)
    {
    }

    virtual const Json::Value &getCustomConfig() const override
    {
        return _jsonConfig["custom_config"];
    }

    virtual PluginBase *getPlugin(const std::string &name) override
    {
        return _pluginsManager.getPlugin(name);
    }

    virtual void addListener(const std::string &ip,
                             uint16_t port,
                             bool useSSL = false,
                             const std::string &certFile = "",
                             const std::string &keyFile = "") override
    {
        assert(!_running);
        _listenerManager.addListener(ip, port, useSSL, certFile, keyFile);
    }
    virtual void setThreadNum(size_t threadNum) override;
    virtual size_t getThreadNum() const override
    {
        return _threadNum;
    }
    virtual void setSSLFiles(const std::string &certPath,
                             const std::string &keyPath) override;
    virtual void run() override;
    virtual void registerWebSocketController(
        const std::string &pathName,
        const std::string &crtlName,
        const std::vector<std::string> &filters =
            std::vector<std::string>()) override;
    virtual void registerHttpSimpleController(
        const std::string &pathName,
        const std::string &crtlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods =
            std::vector<internal::HttpConstraint>{}) override;

    virtual void setCustom404Page(const HttpResponsePtr &resp) override
    {
        resp->setStatusCode(k404NotFound);
        _custom404 = resp;
    }

    const HttpResponsePtr &getCustom404Page()
    {
        return _custom404;
    }

    virtual void forward(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::string &hostString = "") override
    {
        forward(std::dynamic_pointer_cast<HttpRequestImpl>(req),
                std::move(callback),
                hostString);
    }

    void forward(const HttpRequestImplPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 const std::string &hostString);

    virtual void registerBeginningAdvice(
        const std::function<void()> &advice) override
    {
        getLoop()->runInLoop(advice);
    }

    virtual void registerNewConnectionAdvice(
        const std::function<bool(const trantor::InetAddress &,
                                 const trantor::InetAddress &)> &advice)
        override
    {
        _newConnectionAdvices.emplace_back(advice);
    }

    virtual void registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _preRoutingAdvices.emplace_back(advice);
    }
    virtual void registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _postRoutingAdvices.emplace_back(advice);
    }
    virtual void registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _preHandlingAdvices.emplace_back(advice);
    }

    virtual void registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _preRoutingObservers.emplace_back(advice);
    }
    virtual void registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _postRoutingObservers.emplace_back(advice);
    }
    virtual void registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _preHandlingObservers.emplace_back(advice);
    }
    virtual void registerPostHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice) override
    {
        _postHandlingAdvices.emplace_back(advice);
    }

    virtual void enableSession(const size_t timeout = 0) override
    {
        _useSession = true;
        _sessionTimeout = timeout;
    }
    virtual void disableSession() override
    {
        _useSession = false;
    }
    virtual const std::string &getDocumentRoot() const override
    {
        return _rootPath;
    }
    virtual void setDocumentRoot(const std::string &rootPath) override
    {
        _rootPath = rootPath;
    }
    virtual const std::string &getUploadPath() const override
    {
        return _uploadPath;
    }
    virtual const std::shared_ptr<trantor::Resolver> &getResolver() const override
    {
        static auto resolver =
            trantor::Resolver::newResolver(drogon::app().getLoop());
        return resolver;
    }
    virtual void setUploadPath(const std::string &uploadPath) override;
    virtual void setFileTypes(const std::vector<std::string> &types) override;
    virtual void enableDynamicViewsLoading(
        const std::vector<std::string> &libPaths) override;
    virtual void setMaxConnectionNum(size_t maxConnections) override;
    virtual void setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) override;
    virtual void loadConfigFile(const std::string &fileName) override;
    virtual void enableRunAsDaemon() override
    {
        _runAsDaemon = true;
    }
    virtual void enableRelaunchOnError() override
    {
        _relaunchOnError = true;
    }
    virtual void setLogPath(const std::string &logPath,
                            const std::string &logfileBaseName = "",
                            size_t logfileSize = 100000000) override;
    virtual void setLogLevel(trantor::Logger::LogLevel level) override;
    virtual void enableSendfile(bool sendFile) override
    {
        _useSendfile = sendFile;
    }
    virtual void enableGzip(bool useGzip) override
    {
        _useGzip = useGzip;
    }
    virtual bool isGzipEnabled() const override
    {
        return _useGzip;
    }
    virtual void setStaticFilesCacheTime(int cacheTime) override
    {
        _staticFileRouter.setStaticFilesCacheTime(cacheTime);
    }
    virtual int staticFilesCacheTime() const override
    {
        return _staticFileRouter.staticFilesCacheTime();
    }
    virtual void setIdleConnectionTimeout(size_t timeout) override
    {
        _idleConnectionTimeout = timeout;
    }
    virtual void setKeepaliveRequestsNumber(const size_t number) override
    {
        _keepaliveRequestsNumber = number;
    }
    virtual void setPipeliningRequestsNumber(const size_t number) override
    {
        _pipeliningRequestsNumber = number;
    }
    virtual void setGzipStatic(bool useGzipStatic) override
    {
        _staticFileRouter.setGzipStatic(useGzipStatic);
    }
    virtual void setClientMaxBodySize(size_t maxSize) override
    {
        _clientMaxBodySize = maxSize;
    }
    virtual void setClientMaxMemoryBodySize(size_t maxSize) override
    {
        _clientMaxMemoryBodySize = maxSize;
    }
    virtual void setClientMaxWebSocketMessageSize(size_t maxSize) override
    {
        _clientMaxWebSocketMessageSize = maxSize;
    }
    virtual void setHomePage(const std::string &homePageFile) override
    {
        _homePageFile = homePageFile;
    }
    const std::string &getHomePage() const
    {
        return _homePageFile;
    }
    size_t getClientMaxBodySize() const
    {
        return _clientMaxBodySize;
    }
    size_t getClientMaxMemoryBodySize() const
    {
        return _clientMaxMemoryBodySize;
    }
    size_t getClientMaxWebSocketMessageSize() const
    {
        return _clientMaxWebSocketMessageSize;
    }
    virtual std::vector<std::tuple<std::string, HttpMethod, std::string>>
    getHandlersInfo() const override;

    size_t keepaliveRequestsNumber() const
    {
        return _keepaliveRequestsNumber;
    }
    size_t pipeliningRequestsNumber() const
    {
        return _pipeliningRequestsNumber;
    }

    virtual ~HttpAppFrameworkImpl() noexcept
    {
        // Destroy the following objects before _loop destruction
        _sharedLibManagerPtr.reset();
        _sessionManagerPtr.reset();
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

    virtual void enableServerHeader(bool flag) override
    {
        _enableServerHeader = flag;
    }
    virtual void enableDateHeader(bool flag) override
    {
        _enableDateHeader = flag;
    }
    bool sendServerHeader() const
    {
        return _enableServerHeader;
    }
    bool sendDateHeader() const
    {
        return _enableDateHeader;
    }
    const std::string &getServerHeaderString() const
    {
        return _serverHeader;
    }

    virtual orm::DbClientPtr getDbClient(
        const std::string &name = "default") override
    {
        return _dbClientManager.getDbClient(name);
    }
    virtual orm::DbClientPtr getFastDbClient(
        const std::string &name = "default") override
    {
        return _dbClientManager.getFastDbClient(name);
    }
    virtual void createDbClient(const std::string &dbType,
                                const std::string &host,
                                const u_short port,
                                const std::string &databaseName,
                                const std::string &userName,
                                const std::string &password,
                                const size_t connectionNum = 1,
                                const std::string &filename = "",
                                const std::string &name = "default",
                                const bool isFast = false) override
    {
        assert(!_running);
        _dbClientManager.createDbClient(dbType,
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

    inline static HttpAppFrameworkImpl &instance()
    {
        static HttpAppFrameworkImpl _instance;
        return _instance;
    }
    bool useSendfile()
    {
        return _useSendfile;
    }

  private:
    virtual void registerHttpController(
        const std::string &pathPattern,
        const internal::HttpBinderBasePtr &binder,
        const std::vector<HttpMethod> &validMethods = std::vector<HttpMethod>(),
        const std::vector<std::string> &filters = std::vector<std::string>(),
        const std::string &handlerName = "") override;
    void onAsyncRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void onNewWebsockRequest(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const WebSocketConnectionImplPtr &wsConnPtr);
    void onConnection(const trantor::TcpConnectionPtr &conn);
    void addHttpPath(const std::string &path,
                     const internal::HttpBinderBasePtr &binder,
                     const std::vector<HttpMethod> &validMethods,
                     const std::vector<std::string> &filters);

    // We use a uuid string as session id;
    // set _sessionTimeout=0 to make location session valid forever based on
    // cookies;
    size_t _sessionTimeout = 0;
    size_t _idleConnectionTimeout = 60;
    bool _useSession = false;
    ListenerManager _listenerManager;
    std::string _serverHeader =
        "Server: drogon/" + drogon::getVersion() + "\r\n";

    HttpControllersRouter _httpCtrlsRouter;
    HttpSimpleControllersRouter _httpSimpleCtrlsRouter;
    StaticFileRouter _staticFileRouter;
    WebsocketControllersRouter _websockCtrlsRouter;

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
    size_t _pipeliningRequestsNumber = 0;
    bool _useSendfile = true;
    bool _useGzip = true;
    size_t _clientMaxBodySize = 1024 * 1024;
    size_t _clientMaxMemoryBodySize = 64 * 1024;
    size_t _clientMaxWebSocketMessageSize = 128 * 1024;
    std::string _homePageFile = "index.html";
    std::unique_ptr<SessionManager> _sessionManagerPtr;
    // Json::Value _customConfig;
    Json::Value _jsonConfig;
    PluginsManager _pluginsManager;
    HttpResponsePtr _custom404;
    orm::DbClientManager _dbClientManager;
    static InitBeforeMainFunction _initFirst;
    bool _enableServerHeader = true;
    bool _enableDateHeader = true;
    std::vector<std::function<bool(const trantor::InetAddress &,
                                   const trantor::InetAddress &)>>
        _newConnectionAdvices;
    std::vector<std::function<void(const HttpRequestPtr &,
                                   AdviceCallback &&,
                                   AdviceChainCallback &&)>>
        _preRoutingAdvices;
    std::vector<std::function<void(const HttpRequestPtr &,
                                   AdviceCallback &&,
                                   AdviceChainCallback &&)>>
        _postRoutingAdvices;
    std::vector<std::function<void(const HttpRequestPtr &,
                                   AdviceCallback &&,
                                   AdviceChainCallback &&)>>
        _preHandlingAdvices;
    std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        _postHandlingAdvices;

    std::vector<std::function<void(const HttpRequestPtr &)>>
        _preRoutingObservers;
    std::vector<std::function<void(const HttpRequestPtr &)>>
        _postRoutingObservers;
    std::vector<std::function<void(const HttpRequestPtr &)>>
        _preHandlingObservers;
};

}  // namespace drogon
