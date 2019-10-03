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

#include "impl_forwards.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <vector>
#include <functional>
#include <limits>

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
    HttpAppFrameworkImpl();

    virtual const Json::Value &getCustomConfig() const override
    {
        return _jsonConfig["custom_config"];
    }

    virtual PluginBase *getPlugin(const std::string &name) override;
    virtual HttpAppFramework &addListener(
        const std::string &ip,
        uint16_t port,
        bool useSSL = false,
        const std::string &certFile = "",
        const std::string &keyFile = "") override;
    virtual HttpAppFramework &setThreadNum(size_t threadNum) override;
    virtual size_t getThreadNum() const override
    {
        return _threadNum;
    }
    virtual HttpAppFramework &setSSLFiles(const std::string &certPath,
                                          const std::string &keyPath) override;
    virtual void run() override;
    virtual HttpAppFramework &registerWebSocketController(
        const std::string &pathName,
        const std::string &crtlName,
        const std::vector<std::string> &filters =
            std::vector<std::string>()) override;
    virtual HttpAppFramework &registerHttpSimpleController(
        const std::string &pathName,
        const std::string &crtlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods =
            std::vector<internal::HttpConstraint>{}) override;

    virtual HttpAppFramework &setCustom404Page(
        const HttpResponsePtr &resp) override
    {
        resp->setStatusCode(k404NotFound);
        _custom404 = resp;
        return *this;
    }

    const HttpResponsePtr &getCustom404Page()
    {
        return _custom404;
    }

    virtual void forward(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::string &hostString = "") override;

    void forward(const HttpRequestImplPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 const std::string &hostString);

    virtual HttpAppFramework &registerBeginningAdvice(
        const std::function<void()> &advice) override
    {
        _beginningAdvices.emplace_back(advice);
        return *this;
    }

    virtual HttpAppFramework &registerNewConnectionAdvice(
        const std::function<bool(const trantor::InetAddress &,
                                 const trantor::InetAddress &)> &advice)
        override
    {
        _newConnectionAdvices.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerSyncAdvice(
        const std::function<HttpResponsePtr(const HttpRequestPtr &)> &advice)
        override
    {
        _syncAdvices.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _preRoutingAdvices.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _postRoutingAdvices.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override
    {
        _preHandlingAdvices.emplace_back(advice);
        return *this;
    }

    virtual HttpAppFramework &registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _preRoutingObservers.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _postRoutingObservers.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override
    {
        _preHandlingObservers.emplace_back(advice);
        return *this;
    }
    virtual HttpAppFramework &registerPostHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice) override
    {
        _postHandlingAdvices.emplace_back(advice);
        return *this;
    }

    virtual HttpAppFramework &enableSession(const size_t timeout = 0) override
    {
        _useSession = true;
        _sessionTimeout = timeout;
        return *this;
    }
    virtual HttpAppFramework &disableSession() override
    {
        _useSession = false;
        return *this;
    }
    virtual const std::string &getDocumentRoot() const override
    {
        return _rootPath;
    }
    virtual HttpAppFramework &setDocumentRoot(
        const std::string &rootPath) override
    {
        _rootPath = rootPath;
        return *this;
    }
    virtual const std::string &getUploadPath() const override
    {
        return _uploadPath;
    }
    virtual const std::shared_ptr<trantor::Resolver> &getResolver()
        const override
    {
        static auto resolver = trantor::Resolver::newResolver(getLoop());
        return resolver;
    }
    virtual HttpAppFramework &setUploadPath(
        const std::string &uploadPath) override;
    virtual HttpAppFramework &setFileTypes(
        const std::vector<std::string> &types) override;
    virtual HttpAppFramework &enableDynamicViewsLoading(
        const std::vector<std::string> &libPaths) override;
    virtual HttpAppFramework &setMaxConnectionNum(
        size_t maxConnections) override;
    virtual HttpAppFramework &setMaxConnectionNumPerIP(
        size_t maxConnectionsPerIP) override;
    virtual HttpAppFramework &loadConfigFile(
        const std::string &fileName) override;
    virtual HttpAppFramework &enableRunAsDaemon() override
    {
        _runAsDaemon = true;
        return *this;
    }
    virtual HttpAppFramework &enableRelaunchOnError() override
    {
        _relaunchOnError = true;
        return *this;
    }
    virtual HttpAppFramework &setLogPath(
        const std::string &logPath,
        const std::string &logfileBaseName = "",
        size_t logfileSize = 100000000) override;
    virtual HttpAppFramework &setLogLevel(
        trantor::Logger::LogLevel level) override;
    virtual HttpAppFramework &enableSendfile(bool sendFile) override
    {
        _useSendfile = sendFile;
        return *this;
    }
    virtual HttpAppFramework &enableGzip(bool useGzip) override
    {
        _useGzip = useGzip;
        return *this;
    }
    virtual bool isGzipEnabled() const override
    {
        return _useGzip;
    }
    virtual HttpAppFramework &setStaticFilesCacheTime(int cacheTime) override;
    virtual int staticFilesCacheTime() const override;
    virtual HttpAppFramework &setIdleConnectionTimeout(size_t timeout) override
    {
        _idleConnectionTimeout = timeout;
        return *this;
    }
    virtual HttpAppFramework &setKeepaliveRequestsNumber(
        const size_t number) override
    {
        _keepaliveRequestsNumber = number;
        return *this;
    }
    virtual HttpAppFramework &setPipeliningRequestsNumber(
        const size_t number) override
    {
        _pipeliningRequestsNumber = number;
        return *this;
    }
    virtual HttpAppFramework &setGzipStatic(bool useGzipStatic) override;
    virtual HttpAppFramework &setClientMaxBodySize(size_t maxSize) override
    {
        _clientMaxBodySize = maxSize;
        return *this;
    }
    virtual HttpAppFramework &setClientMaxMemoryBodySize(
        size_t maxSize) override
    {
        _clientMaxMemoryBodySize = maxSize;
        return *this;
    }
    virtual HttpAppFramework &setClientMaxWebSocketMessageSize(
        size_t maxSize) override
    {
        _clientMaxWebSocketMessageSize = maxSize;
        return *this;
    }
    virtual HttpAppFramework &setHomePage(
        const std::string &homePageFile) override
    {
        _homePageFile = homePageFile;
        return *this;
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

    virtual ~HttpAppFrameworkImpl() noexcept;
    virtual bool isRunning() override
    {
        return _running;
    }

    virtual trantor::EventLoop *getLoop() const override;

    virtual void quit() override
    {
        if (getLoop()->isRunning())
            getLoop()->quit();
    }

    virtual HttpAppFramework &setServerHeaderField(
        const std::string &server) override
    {
        assert(!_running);
        assert(server.find("\r\n") == std::string::npos);
        _serverHeader = "Server: " + server + "\r\n";
        return *this;
    }

    virtual HttpAppFramework &enableServerHeader(bool flag) override
    {
        _enableServerHeader = flag;
        return *this;
    }
    virtual HttpAppFramework &enableDateHeader(bool flag) override
    {
        _enableDateHeader = flag;
        return *this;
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
        const std::string &name = "default") override;
    virtual orm::DbClientPtr getFastDbClient(
        const std::string &name = "default") override;
    virtual HttpAppFramework &createDbClient(
        const std::string &dbType,
        const std::string &host,
        const u_short port,
        const std::string &databaseName,
        const std::string &userName,
        const std::string &password,
        const size_t connectionNum = 1,
        const std::string &filename = "",
        const std::string &name = "default",
        const bool isFast = false) override;

    inline static HttpAppFrameworkImpl &instance()
    {
        static HttpAppFrameworkImpl _instance;
        return _instance;
    }
    bool useSendfile()
    {
        return _useSendfile;
    }
    void callCallback(
        const HttpRequestImplPtr &req,
        const HttpResponsePtr &resp,
        const std::function<void(const HttpResponsePtr &)> &callback);

    virtual bool supportSSL() const override
    {
#ifdef OpenSSL_FOUND
        return true;
#endif
        return false;
    }

    virtual size_t getCurrentThreadIndex() const override
    {
        auto *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
        if (loop)
        {
            return loop->index();
        }
        return std::numeric_limits<size_t>::max();
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
    std::string _serverHeader =
        "Server: drogon/" + drogon::getVersion() + "\r\n";

    const std::unique_ptr<StaticFileRouter> _staticFileRouterPtr;
    const std::unique_ptr<HttpControllersRouter> _httpCtrlsRouterPtr;
    const std::unique_ptr<HttpSimpleControllersRouter>
        _httpSimpleCtrlsRouterPtr;
    const std::unique_ptr<WebsocketControllersRouter> _websockCtrlsRouterPtr;

    const std::unique_ptr<ListenerManager> _listenerManagerPtr;
    const std::unique_ptr<PluginsManager> _pluginsManagerPtr;
    const std::unique_ptr<orm::DbClientManager> _dbClientManagerPtr;

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
    HttpResponsePtr _custom404;
    static InitBeforeMainFunction _initFirst;
    bool _enableServerHeader = true;
    bool _enableDateHeader = true;
    std::vector<std::function<void()>> _beginningAdvices;
    std::vector<std::function<bool(const trantor::InetAddress &,
                                   const trantor::InetAddress &)>>
        _newConnectionAdvices;
    std::vector<std::function<HttpResponsePtr(const HttpRequestPtr &)>>
        _syncAdvices;
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
