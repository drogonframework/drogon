/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <drogon/config.h>
#if USE_POSTGRESQL
#include <drogon/orm/DbClient.h>
#endif
#include <drogon/utils/Utilities.h>
#include <drogon/HttpBinder.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/PostFilter.h>
#include <drogon/GetFilter.h>
#include <drogon/PutFilter.h>
#include <drogon/DeleteFilter.h>
#include <drogon/LocalHostFilter.h>
#include <drogon/InnerIpFilter.h>
#include <drogon/version.h>
#include <drogon/NotFound.h>
#include <drogon/HttpClient.h>
#include <drogon/FileUpload.h>
#include <trantor/net/EventLoop.h>
#include <drogon/CacheMap.h>
#include <memory>
#include <string>
#include <functional>
#include <vector>
namespace drogon
{
//the drogon banner
const char banner[] = "     _                             \n"
                      "  __| |_ __ ___   __ _  ___  _ __  \n"
                      " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                      "| (_| | | | (_) | (_| | (_) | | | |\n"
                      " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                      "                 |___/             \n";
inline std::string getVersion()
{
    return VERSION;
}
inline std::string getGitCommit()
{
    return VERSION_MD5;
}

class HttpAppFramework : public trantor::NonCopyable
{
  public:
    static HttpAppFramework &instance();
    virtual void setThreadNum(size_t threadNum) = 0;
    virtual void setSSLFiles(const std::string &certPath,
                             const std::string &keyPath) = 0;
    virtual void addListener(const std::string &ip,
                             uint16_t port,
                             bool useSSL = false,
                             const std::string &certFile = "",
                             const std::string &keyFile = "") = 0;
    virtual void run() = 0;
    virtual ~HttpAppFramework();
    virtual void registerWebSocketController(const std::string &pathName,
                                             const std::string &crtlName,
                                             const std::vector<std::string> &filters =
                                                 std::vector<std::string>()) = 0;
    virtual void registerHttpSimpleController(const std::string &pathName,
                                              const std::string &crtlName,
                                              const std::vector<any> &filtersAndMethods = std::vector<any>()) = 0;
    template <typename FUNCTION>
    void registerHttpMethod(const std::string &pathPattern,
                               FUNCTION &&function,
                               const std::vector<any> &filtersAndMethods = std::vector<any>())
    {
        LOG_TRACE << "pathPattern:" << pathPattern;
        HttpBinderBasePtr binder;

        binder = std::make_shared<
            HttpBinder<FUNCTION>>(std::forward<FUNCTION>(function));

        std::vector<HttpMethod> validMethods;
        std::vector<std::string> filters;
        for (auto &filterOrMethod : filtersAndMethods)
        {
            if (filterOrMethod.type() == typeid(std::string))
            {
                filters.push_back(*any_cast<std::string>(&filterOrMethod));
            }
            else if (filterOrMethod.type() == typeid(const char *))
            {
                filters.push_back(*any_cast<const char *>(&filterOrMethod));
            }
            else if (filterOrMethod.type() == typeid(HttpMethod))
            {
                validMethods.push_back(*any_cast<HttpMethod>(&filterOrMethod));
            }
            else
            {
                std::cerr << "Invalid controller constraint type:" << filterOrMethod.type().name() << std::endl;
                LOG_ERROR << "Invalid controller constraint type";
                exit(1);
            }
        }

        registerHttpController(pathPattern, binder, validMethods, filters);
    }
    virtual void enableSession(const size_t timeout = 0) = 0;
    virtual void disableSession() = 0;
    virtual const std::string &getDocumentRoot() const = 0;
    virtual void setDocumentRoot(const std::string &rootPath) = 0;
    virtual void setFileTypes(const std::vector<std::string> &types) = 0;
    virtual void enableDynamicViewsLoading(const std::vector<std::string> &libPaths) = 0;

    virtual void setMaxConnectionNum(size_t maxConnections) = 0;
    virtual void setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) = 0;

    virtual void loadConfigFile(const std::string &fileName) = 0;

    virtual void enableRunAsDaemon() = 0;
    virtual void enableRelaunchOnError() = 0;

    virtual void setLogPath(const std::string &logPath,
                            const std::string &logfileBaseName = "",
                            size_t logSize = 100000000) = 0;
    virtual void enableSendfile(bool sendFile) = 0;
    virtual void enableGzip(bool useGzip) = 0;
    virtual bool useGzip() const = 0;
    virtual void setStaticFilesCacheTime(int cacheTime) = 0;
    virtual int staticFilesCacheTime() const = 0;
    virtual void setIdleConnectionTimeout(size_t timeout) = 0;
#if USE_POSTGRESQL
    virtual orm::DbClientPtr getDbClient(const std::string &name = "default") = 0;
    virtual void createDbClient(const std::string &dbType,
                                const std::string &host,
                                const u_short port,
                                const std::string &databaseName,
                                const std::string &userName,
                                const std::string &password,
                                const size_t connectionNum = 1,
                                const std::string &name = "default") = 0;
#endif

  private:
    virtual void registerHttpController(const std::string &pathPattern,
                                           const HttpBinderBasePtr &binder,
                                           const std::vector<HttpMethod> &validMethods = std::vector<HttpMethod>(),
                                           const std::vector<std::string> &filters = std::vector<std::string>()) = 0;
};

inline HttpAppFramework &app()
{
    return HttpAppFramework::instance();
}

} // namespace drogon
