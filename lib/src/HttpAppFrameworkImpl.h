/**
 *
 *  @file HttpAppFrameworkImpl.h
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

#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/config.h>
#include <json/json.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "SessionManager.h"
#include "drogon/utils/Utilities.h"
#include "impl_forwards.h"

namespace trantor
{
class EventLoopThreadPool;
}

namespace drogon
{
HttpResponsePtr defaultErrorHandler(HttpStatusCode code,
                                    const HttpRequestPtr &req);
void defaultExceptionHandler(const std::exception &,
                             const HttpRequestPtr &,
                             std::function<void(const HttpResponsePtr &)> &&);

struct InitBeforeMainFunction
{
    explicit InitBeforeMainFunction(const std::function<void()> &func)
    {
        func();
    }
};

class HttpAppFrameworkImpl final : public HttpAppFramework
{
  public:
    HttpAppFrameworkImpl();

    const Json::Value &getCustomConfig() const override
    {
        return jsonConfig_["custom_config"];
    }

    PluginBase *getPlugin(const std::string &name) override;
    std::shared_ptr<PluginBase> getSharedPlugin(
        const std::string &name) override;
    void addPlugins(const Json::Value &configs) override;
    void addPlugin(const std::string &name,
                   const std::vector<std::string> &dependencies,
                   const Json::Value &config) override;
    HttpAppFramework &addListener(
        const std::string &ip,
        uint16_t port,
        bool useSSL,
        const std::string &certFile,
        const std::string &keyFile,
        bool useOldTLS,
        const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
        override;
    HttpAppFramework &setThreadNum(size_t threadNum) override;

    size_t getThreadNum() const override
    {
        return threadNum_;
    }

    HttpAppFramework &setSSLConfigCommands(
        const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
        override;
    HttpAppFramework &setSSLFiles(const std::string &certPath,
                                  const std::string &keyPath) override;
    void run() override;
    HttpAppFramework &registerWebSocketController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods)
        override;
    HttpAppFramework &registerHttpSimpleController(
        const std::string &pathName,
        const std::string &ctrlName,
        const std::vector<internal::HttpConstraint> &filtersAndMethods)
        override;

    HttpAppFramework &setCustom404Page(const HttpResponsePtr &resp,
                                       bool set404) override
    {
        if (set404)
        {
            resp->setStatusCode(k404NotFound);
        }
        custom404_ = resp;
        return *this;
    }

    HttpAppFramework &setCustomErrorHandler(
        std::function<HttpResponsePtr(HttpStatusCode,
                                      const HttpRequestPtr &req)>
            &&resp_generator) override;

    const HttpResponsePtr &getCustom404Page();

    void forward(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 const std::string &hostString,
                 double timeout) override;

    void forward(const HttpRequestImplPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 const std::string &hostString,
                 double timeout = 0);

    HttpAppFramework &registerBeginningAdvice(
        const std::function<void()> &advice) override
    {
        beginningAdvices_.emplace_back(advice);
        return *this;
    }

    HttpAppFramework &registerNewConnectionAdvice(
        const std::function<bool(const trantor::InetAddress &,
                                 const trantor::InetAddress &)> &advice)
        override;

    HttpAppFramework &registerHttpResponseCreationAdvice(
        const std::function<void(const HttpResponsePtr &)> &advice) override;

    HttpAppFramework &registerSyncAdvice(
        const std::function<HttpResponsePtr(const HttpRequestPtr &)> &advice)
        override;

    HttpAppFramework &registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override;

    HttpAppFramework &registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override;

    HttpAppFramework &registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice) override;

    HttpAppFramework &registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override;

    HttpAppFramework &registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override;

    HttpAppFramework &registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &)> &advice) override;

    HttpAppFramework &registerPostHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice) override;

    HttpAppFramework &registerPreSendingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice) override;

    HttpAppFramework &setDefaultHandler(DefaultHandler handler) override;

    HttpAppFramework &setupFileLogger() override;

    HttpAppFramework &enableSession(
        const size_t timeout,
        Cookie::SameSite sameSite = Cookie::SameSite::kNull,
        const std::string &cookieKey = "JSESSIONID",
        int maxAge = -1,
        SessionManager::IdGeneratorCallback idGeneratorCallback =
            nullptr) override
    {
        useSession_ = true;
        sessionTimeout_ = timeout;
        sessionSameSite_ = sameSite;
        sessionCookieKey_ = cookieKey;
        sessionMaxAge_ = maxAge;
        return setSessionIdGenerator(idGeneratorCallback);
    }

    HttpAppFramework &setSessionIdGenerator(
        SessionManager::IdGeneratorCallback idGeneratorCallback = nullptr)
    {
        sessionIdGeneratorCallback_ =
            idGeneratorCallback ? idGeneratorCallback
                                : []() { return utils::getUuid(true); };
        return *this;
    }

    HttpAppFramework &disableSession() override
    {
        useSession_ = false;
        return *this;
    }

    HttpAppFramework &registerSessionStartAdvice(
        const AdviceStartSessionCallback &advice) override
    {
        sessionStartAdvices_.emplace_back(advice);
        return *this;
    }

    HttpAppFramework &registerSessionDestroyAdvice(
        const AdviceDestroySessionCallback &advice) override
    {
        sessionDestroyAdvices_.emplace_back(advice);
        return *this;
    }

    const std::string &getDocumentRoot() const override
    {
        return rootPath_;
    }

    HttpAppFramework &setDocumentRoot(const std::string &rootPath) override
    {
        rootPath_ = rootPath;
        return *this;
    }

    HttpAppFramework &setStaticFileHeaders(
        const std::vector<std::pair<std::string, std::string>> &headers)
        override;

    HttpAppFramework &addALocation(
        const std::string &uriPrefix,
        const std::string &defaultContentType,
        const std::string &alias,
        bool isCaseSensitive,
        bool allowAll,
        bool isRecursive,
        const std::vector<std::string> &filters) override;

    const std::string &getUploadPath() const override
    {
        return uploadPath_;
    }

    const std::shared_ptr<trantor::Resolver> &getResolver() const override
    {
        static auto resolver = trantor::Resolver::newResolver(getLoop());
        return resolver;
    }

    HttpAppFramework &setUploadPath(const std::string &uploadPath) override;
    HttpAppFramework &setFileTypes(
        const std::vector<std::string> &types) override;
#ifndef _WIN32
    HttpAppFramework &enableDynamicViewsLoading(
        const std::vector<std::string> &libPaths,
        const std::string &outputPath) override;
#endif
    HttpAppFramework &setMaxConnectionNum(size_t maxConnections) override;
    HttpAppFramework &setMaxConnectionNumPerIP(
        size_t maxConnectionsPerIP) override;
    HttpAppFramework &loadConfigFile(const std::string &fileName) noexcept(
        false) override;
    HttpAppFramework &loadConfigJson(const Json::Value &data) noexcept(
        false) override;
    HttpAppFramework &loadConfigJson(Json::Value &&data) noexcept(
        false) override;

    HttpAppFramework &enableRunAsDaemon() override
    {
        runAsDaemon_ = true;
        return *this;
    }

    HttpAppFramework &disableSigtermHandling() override
    {
        handleSigterm_ = false;
        return *this;
    }

    HttpAppFramework &enableRelaunchOnError() override
    {
        relaunchOnError_ = true;
        return *this;
    }

    HttpAppFramework &setLogPath(const std::string &logPath,
                                 const std::string &logfileBaseName,
                                 size_t logfileSize,
                                 size_t maxFiles,
                                 bool useSpdlog) override;
    HttpAppFramework &setLogLevel(trantor::Logger::LogLevel level) override;
    HttpAppFramework &setLogLocalTime(bool on) override;

    HttpAppFramework &enableSendfile(bool sendFile) override
    {
        useSendfile_ = sendFile;
        return *this;
    }

    HttpAppFramework &enableGzip(bool useGzip) override
    {
        useGzip_ = useGzip;
        return *this;
    }

    bool isGzipEnabled() const override
    {
        return useGzip_;
    }

    HttpAppFramework &enableBrotli(bool useBrotli) override
    {
        useBrotli_ = useBrotli;
        return *this;
    }

    bool isBrotliEnabled() const override
    {
        return useBrotli_;
    }

    HttpAppFramework &setStaticFilesCacheTime(int cacheTime) override;
    int staticFilesCacheTime() const override;

    HttpAppFramework &setIdleConnectionTimeout(size_t timeout) override
    {
        idleConnectionTimeout_ = timeout;
        return *this;
    }

    size_t getIdleConnectionTimeout() const  // could expose in base class
    {
        return idleConnectionTimeout_;
    }

    HttpAppFramework &setKeepaliveRequestsNumber(const size_t number) override
    {
        keepaliveRequestsNumber_ = number;
        return *this;
    }

    HttpAppFramework &setPipeliningRequestsNumber(const size_t number) override
    {
        pipeliningRequestsNumber_ = number;
        return *this;
    }

    HttpAppFramework &setGzipStatic(bool useGzipStatic) override;
    HttpAppFramework &setBrStatic(bool useGzipStatic) override;

    HttpAppFramework &setClientMaxBodySize(size_t maxSize) override
    {
        clientMaxBodySize_ = maxSize;
        return *this;
    }

    HttpAppFramework &setClientMaxMemoryBodySize(size_t maxSize) override
    {
        clientMaxMemoryBodySize_ = maxSize;
        return *this;
    }

    HttpAppFramework &setClientMaxWebSocketMessageSize(size_t maxSize) override
    {
        clientMaxWebSocketMessageSize_ = maxSize;
        return *this;
    }

    HttpAppFramework &setHomePage(const std::string &homePageFile) override
    {
        homePageFile_ = homePageFile;
        return *this;
    }

    const std::string &getHomePage() const override
    {
        return homePageFile_;
    }

    HttpAppFramework &setTermSignalHandler(
        const std::function<void()> &handler) override
    {
        termSignalHandler_ = handler;
        return *this;
    }

    const std::function<void()> &getTermSignalHandler() const
    {
        return termSignalHandler_;
    }

    HttpAppFramework &setIntSignalHandler(
        const std::function<void()> &handler) override
    {
        intSignalHandler_ = handler;
        return *this;
    }

    const std::function<void()> &getIntSignalHandler() const
    {
        return intSignalHandler_;
    }

    HttpAppFramework &setImplicitPageEnable(bool useImplicitPage) override;
    bool isImplicitPageEnabled() const override;
    HttpAppFramework &setImplicitPage(
        const std::string &implicitPageFile) override;
    const std::string &getImplicitPage() const override;

    size_t getClientMaxBodySize() const
    {
        return clientMaxBodySize_;
    }

    size_t getClientMaxMemoryBodySize() const
    {
        return clientMaxMemoryBodySize_;
    }

    size_t getClientMaxWebSocketMessageSize() const
    {
        return clientMaxWebSocketMessageSize_;
    }

    std::vector<HttpHandlerInfo> getHandlersInfo() const override;

    size_t keepaliveRequestsNumber() const
    {
        return keepaliveRequestsNumber_;
    }

    size_t pipeliningRequestsNumber() const
    {
        return pipeliningRequestsNumber_;
    }

    ~HttpAppFrameworkImpl() noexcept override;

    bool isRunning() override
    {
        return running_;
    }

    HttpAppFramework &setJsonParserStackLimit(size_t limit) noexcept override
    {
        jsonStackLimit_ = limit;
        return *this;
    }

    size_t getJsonParserStackLimit() const noexcept override
    {
        return jsonStackLimit_;
    }

    HttpAppFramework &setUnicodeEscapingInJson(bool enable) noexcept override
    {
        usingUnicodeEscaping_ = enable;
        return *this;
    }

    bool isUnicodeEscapingUsedInJson() const noexcept override
    {
        return usingUnicodeEscaping_;
    }

    HttpAppFramework &setFloatPrecisionInJson(
        unsigned int precision,
        const std::string &precisionType) noexcept override
    {
        floatPrecisionInJson_ = std::make_pair(precision, precisionType);
        return *this;
    }

    const std::pair<unsigned int, std::string> &getFloatPrecisionInJson()
        const noexcept override
    {
        return floatPrecisionInJson_;
    }

    trantor::EventLoop *getLoop() const override;

    trantor::EventLoop *getIOLoop(size_t id) const override;

    void quit() override;

    HttpAppFramework &setServerHeaderField(const std::string &server) override
    {
        assert(!running_);
        assert(server.find("\r\n") == std::string::npos);
        serverHeader_ = "server: " + server + "\r\n";
        return *this;
    }

    HttpAppFramework &enableServerHeader(bool flag) override
    {
        enableServerHeader_ = flag;
        return *this;
    }

    HttpAppFramework &enableDateHeader(bool flag) override
    {
        enableDateHeader_ = flag;
        return *this;
    }

    bool sendServerHeader() const
    {
        return enableServerHeader_;
    }

    bool sendDateHeader() const
    {
        return enableDateHeader_;
    }

    const std::string &getServerHeaderString() const
    {
        return serverHeader_;
    }

    orm::DbClientPtr getDbClient(const std::string &name) override;
    orm::DbClientPtr getFastDbClient(const std::string &name) override;
    HttpAppFramework &createDbClient(const std::string &dbType,
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
                                     const bool autoBatch) override;
    HttpAppFramework &createRedisClient(const std::string &ip,
                                        unsigned short port,
                                        const std::string &name,
                                        const std::string &password,
                                        size_t connectionNum,
                                        bool isFast,
                                        double timeout,
                                        unsigned int db,
                                        const std::string &username) override;
    nosql::RedisClientPtr getRedisClient(const std::string &name) override;
    nosql::RedisClientPtr getFastRedisClient(const std::string &name) override;
    std::vector<trantor::InetAddress> getListeners() const override;

    inline static HttpAppFrameworkImpl &instance()
    {
        static HttpAppFrameworkImpl instance;
        return instance;
    }

    bool useSendfile() const
    {
        return useSendfile_;
    }

    bool supportSSL() const override
    {
        return trantor::utils::tlsBackend() != "None";
    }

    size_t getCurrentThreadIndex() const override
    {
        auto *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
        if (loop)
        {
            return loop->index();
        }
        return (std::numeric_limits<size_t>::max)();
    }

    bool areAllDbClientsAvailable() const noexcept override;
    const std::function<HttpResponsePtr(HttpStatusCode,
                                        const HttpRequestPtr &req)> &
    getCustomErrorHandler() const override;

    bool isUsingCustomErrorHandler() const
    {
        return usingCustomErrorHandler_;
    }

    void enableReusePort(bool enable) override
    {
        reusePort_ = enable;
    }

    bool reusePort() const override
    {
        return reusePort_;
    }

    HttpAppFramework &setExceptionHandler(ExceptionHandler handler) override
    {
        exceptionHandler_ = std::move(handler);
        return *this;
    }

    const ExceptionHandler &getExceptionHandler() const override
    {
        return exceptionHandler_;
    }

    HttpAppFramework &enableCompressedRequest(bool enable) override
    {
        enableCompressedRequest_ = enable;
        return *this;
    }

    bool isCompressedRequestEnabled() const override
    {
        return enableCompressedRequest_;
    }

    HttpAppFramework &registerCustomExtensionMime(
        const std::string &ext,
        const std::string &mime) override;

    // should return unsigned type!
    int64_t getConnectionCount() const override;

    // TODO: move session related codes to its own singleton class
    void findSessionForRequest(const HttpRequestImplPtr &req);
    HttpResponsePtr handleSessionForResponse(const HttpRequestImplPtr &req,
                                             const HttpResponsePtr &resp);

  private:
    void registerHttpController(const std::string &pathPattern,
                                const internal::HttpBinderBasePtr &binder,
                                const std::vector<HttpMethod> &validMethods,
                                const std::vector<std::string> &filters,
                                const std::string &handlerName) override;
    void registerHttpControllerViaRegex(
        const std::string &regExp,
        const internal::HttpBinderBasePtr &binder,
        const std::vector<HttpMethod> &validMethods,
        const std::vector<std::string> &filters,
        const std::string &handlerName) override;

    // We use an uuid string as session id;
    // set sessionTimeout_=0 to make location session valid forever based on
    // cookies;
    size_t sessionTimeout_{0};
    Cookie::SameSite sessionSameSite_{Cookie::SameSite::kNull};
    std::string sessionCookieKey_{"JSESSIONID"};
    int sessionMaxAge_{-1};
    size_t idleConnectionTimeout_{60};
    bool useSession_{false};
    std::string serverHeader_{"server: drogon/" + drogon::getVersion() +
                              "\r\n"};

    std::unique_ptr<ListenerManager> listenerManagerPtr_;
    std::unique_ptr<PluginsManager> pluginsManagerPtr_;
    std::unique_ptr<orm::DbClientManager> dbClientManagerPtr_;
    std::unique_ptr<nosql::RedisClientManager> redisClientManagerPtr_;

    std::string rootPath_{"./"};
    std::string uploadPath_;
    std::atomic_bool running_{false};
    std::atomic_bool routersInit_{false};

    size_t threadNum_{1};
    std::unique_ptr<trantor::EventLoopThreadPool> ioLoopThreadPool_;

#ifndef _WIN32
    std::vector<std::string> libFilePaths_;
    std::string libFileOutputPath_;
    std::unique_ptr<SharedLibManager> sharedLibManagerPtr_;
#endif

    std::vector<std::pair<std::string, std::string>> sslConfCmds_;
    std::string sslCertPath_;
    std::string sslKeyPath_;

    bool runAsDaemon_{false};
    bool handleSigterm_{true};
    bool relaunchOnError_{false};
    bool logWithSpdlog_{false};
    std::string logPath_;
    std::string logfileBaseName_;
    size_t logfileSize_{100000000};
    size_t logfileMaxNum_{0};
    size_t keepaliveRequestsNumber_{0};
    size_t pipeliningRequestsNumber_{0};
    size_t jsonStackLimit_{1000};
    bool useSendfile_{true};
    bool useGzip_{true};
    bool useBrotli_{false};
    bool usingUnicodeEscaping_{true};
    std::pair<unsigned int, std::string> floatPrecisionInJson_{0,
                                                               "significant"};
    bool usingCustomErrorHandler_{false};
    size_t clientMaxBodySize_{1024 * 1024};
    size_t clientMaxMemoryBodySize_{64 * 1024};
    size_t clientMaxWebSocketMessageSize_{128 * 1024};
    std::string homePageFile_{"index.html"};
    std::function<void()> termSignalHandler_{[]() { app().quit(); }};
    std::function<void()> intSignalHandler_{[]() { app().quit(); }};
    std::unique_ptr<SessionManager> sessionManagerPtr_;
    std::vector<AdviceStartSessionCallback> sessionStartAdvices_;
    std::vector<AdviceDestroySessionCallback> sessionDestroyAdvices_;
    SessionManager::IdGeneratorCallback sessionIdGeneratorCallback_;
    std::shared_ptr<trantor::AsyncFileLogger> asyncFileLoggerPtr_;
    Json::Value jsonConfig_;
    Json::Value jsonRuntimeConfig_;
    HttpResponsePtr custom404_;
    std::function<HttpResponsePtr(HttpStatusCode, const HttpRequestPtr &req)>
        customErrorHandler_ = &defaultErrorHandler;
    static InitBeforeMainFunction initFirst_;
    bool enableServerHeader_{true};
    bool enableDateHeader_{true};
    bool reusePort_{false};
    std::vector<std::function<void()>> beginningAdvices_;

    ExceptionHandler exceptionHandler_{defaultExceptionHandler};
    bool enableCompressedRequest_{false};
};

}  // namespace drogon
