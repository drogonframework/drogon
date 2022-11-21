/**
 *
 *  @file ConfigLoader.cc
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

#include "ConfigLoader.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/config.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <trantor/utils/Logger.h>
#if !defined(_WIN32) || defined(__MINGW32__)
#include <unistd.h>
#define os_access access
#else
#include <io.h>
#define os_access _waccess
#define R_OK 04
#define W_OK 02
#endif
#include <drogon/utils/Utilities.h>

using namespace drogon;
static bool bytesSize(std::string &sizeStr, size_t &size)
{
    if (sizeStr.empty())
    {
        size = -1;
        return true;
    }
    else
    {
        size = 1;
        switch (sizeStr[sizeStr.length() - 1])
        {
            case 'k':
            case 'K':
                size = 1024;
                sizeStr.resize(sizeStr.length() - 1);
                break;
            case 'M':
            case 'm':
                size = (1024 * 1024);
                sizeStr.resize(sizeStr.length() - 1);
                break;
            case 'g':
            case 'G':
                size = (1024 * 1024 * 1024);
                sizeStr.resize(sizeStr.length() - 1);
                break;
#if ((ULONG_MAX) != (UINT_MAX))
            // 64bit system
            case 't':
            case 'T':
                size = (1024L * 1024L * 1024L * 1024L);
                sizeStr.resize(sizeStr.length() - 1);
                break;
#endif
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '7':
            case '8':
            case '9':
                break;
            default:
                return false;
                break;
        }
        std::istringstream iss(sizeStr);
        size_t tmpSize;
        iss >> tmpSize;
        if (iss.fail())
        {
            return false;
        }
        if ((size_t(-1) / tmpSize) >= size)
            size *= tmpSize;
        else
        {
            size = -1;
        }
        return true;
    }
}
ConfigLoader::ConfigLoader(const std::string &configFile)
{
    if (os_access(drogon::utils::toNativePath(configFile).c_str(), 0) != 0)
    {
        throw std::runtime_error("Config file " + configFile + " not found!");
    }
    if (os_access(drogon::utils::toNativePath(configFile).c_str(), R_OK) != 0)
    {
        throw std::runtime_error("No permission to read config file " +
                                 configFile);
    }
    configFile_ = configFile;
    try
    {
        std::ifstream infile(drogon::utils::toNativePath(configFile).c_str(),
                             std::ifstream::in);
        if (infile)
        {
            infile >> configJsonRoot_;
        }
    }
    catch (std::exception &e)
    {
        throw std::runtime_error("Error reading config file " + configFile +
                                 ": " + e.what());
    }
}
ConfigLoader::ConfigLoader(const Json::Value &data) : configJsonRoot_(data)
{
}
ConfigLoader::ConfigLoader(Json::Value &&data)
    : configJsonRoot_(std::move(data))
{
}
ConfigLoader::~ConfigLoader()
{
}
static void loadLogSetting(const Json::Value &log)
{
    if (!log)
        return;
    auto logPath = log.get("log_path", "").asString();
    if (logPath != "")
    {
        auto baseName = log.get("logfile_base_name", "").asString();
        auto logSize = log.get("log_size_limit", 100000000).asUInt64();
        HttpAppFrameworkImpl::instance().setLogPath(logPath, baseName, logSize);
    }
    auto logLevel = log.get("log_level", "DEBUG").asString();
    if (logLevel == "TRACE")
    {
        trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    }
    else if (logLevel == "DEBUG")
    {
        trantor::Logger::setLogLevel(trantor::Logger::kDebug);
    }
    else if (logLevel == "INFO")
    {
        trantor::Logger::setLogLevel(trantor::Logger::kInfo);
    }
    else if (logLevel == "WARN")
    {
        trantor::Logger::setLogLevel(trantor::Logger::kWarn);
    }
}
static void loadControllers(const Json::Value &controllers)
{
    if (!controllers)
        return;
    for (auto const &controller : controllers)
    {
        auto path = controller.get("path", "").asString();
        auto ctrlName = controller.get("controller", "").asString();
        if (path == "" || ctrlName == "")
            continue;
        std::vector<internal::HttpConstraint> constraints;
        if (!controller["http_methods"].isNull())
        {
            for (auto const &method : controller["http_methods"])
            {
                auto strMethod = method.asString();
                std::transform(strMethod.begin(),
                               strMethod.end(),
                               strMethod.begin(),
                               [](unsigned char c) { return tolower(c); });
                if (strMethod == "get")
                {
                    constraints.push_back(Get);
                }
                else if (strMethod == "post")
                {
                    constraints.push_back(Post);
                }
                else if (strMethod == "head")  // The branch nerver work
                {
                    constraints.push_back(Head);
                }
                else if (strMethod == "put")
                {
                    constraints.push_back(Put);
                }
                else if (strMethod == "delete")
                {
                    constraints.push_back(Delete);
                }
                else if (strMethod == "patch")
                {
                    constraints.push_back(Patch);
                }
            }
        }
        if (!controller["filters"].isNull())
        {
            for (auto const &filter : controller["filters"])
            {
                constraints.push_back(filter.asString());
            }
        }
        drogon::app().registerHttpSimpleController(path, ctrlName, constraints);
    }
}
static void loadApp(const Json::Value &app)
{
    if (!app)
        return;
    // threads number
    auto threadsNum = app.get("threads_num", 1).asUInt64();
    if (threadsNum == 1)
    {
        threadsNum = app.get("number_of_threads", 1).asUInt64();
    }
    if (threadsNum == 0)
    {
        // set the number to the number of processors.
        threadsNum = std::thread::hardware_concurrency();
        LOG_TRACE << "The number of processors is " << threadsNum;
    }
    if (threadsNum < 1)
        threadsNum = 1;
    drogon::app().setThreadNum(threadsNum);
    // session
    auto enableSession = app.get("enable_session", false).asBool();
    auto timeout = app.get("session_timeout", 0).asUInt64();
    auto sameSite = app.get("session_same_site", "Null").asString();
    if (enableSession)
        drogon::app().enableSession(timeout,
                                    Cookie::convertString2SameSite(sameSite));
    else
        drogon::app().disableSession();
    // document root
    auto documentRoot = app.get("document_root", "").asString();
    if (documentRoot != "")
    {
        drogon::app().setDocumentRoot(documentRoot);
    }
    if (!app["static_file_headers"].empty())
    {
        if (app["static_file_headers"].isArray())
        {
            std::vector<std::pair<std::string, std::string>> headers;
            for (auto &header : app["static_file_headers"])
            {
                headers.emplace_back(std::make_pair<std::string, std::string>(
                    header["name"].asString(), header["value"].asString()));
            }
            drogon::app().setStaticFileHeaders(headers);
        }
        else
        {
            throw std::runtime_error(
                "The static_file_headers option must be an array");
        }
    }
    // upload path
    auto uploadPath = app.get("upload_path", "uploads").asString();
    drogon::app().setUploadPath(uploadPath);
    // file types
    auto fileTypes = app["file_types"];
    if (fileTypes.isArray() && !fileTypes.empty())
    {
        std::vector<std::string> types;
        for (auto const &fileType : fileTypes)
        {
            types.push_back(fileType.asString());
            LOG_TRACE << "file type:" << types.back();
        }
        drogon::app().setFileTypes(types);
    }
    // locations
    if (app.isMember("locations"))
    {
        auto &locations = app["locations"];
        if (!locations.isArray())
        {
            throw std::runtime_error("The locations option must be an array");
        }
        for (auto &location : locations)
        {
            auto uri = location.get("uri_prefix", "").asString();
            if (uri.empty())
                continue;
            auto defaultContentType =
                location.get("default_content_type", "").asString();
            auto alias = location.get("alias", "").asString();
            auto isCaseSensitive =
                location.get("is_case_sensitive", false).asBool();
            auto allAll = location.get("allow_all", true).asBool();
            auto isRecursive = location.get("is_recursive", true).asBool();
            if (!location["filters"].isNull())
            {
                if (location["filters"].isArray())
                {
                    std::vector<std::string> filters;
                    for (auto const &filter : location["filters"])
                    {
                        filters.push_back(filter.asString());
                    }
                    drogon::app().addALocation(uri,
                                               defaultContentType,
                                               alias,
                                               isCaseSensitive,
                                               allAll,
                                               isRecursive,
                                               filters);
                }
                else
                {
                    throw std::runtime_error("the filters of location '" + uri +
                                             "' should be an array");
                }
            }
            else
            {
                drogon::app().addALocation(uri,
                                           defaultContentType,
                                           alias,
                                           isCaseSensitive,
                                           allAll,
                                           isRecursive);
            }
        }
    }
    // max connections
    auto maxConns = app.get("max_connections", 0).asUInt64();
    if (maxConns > 0)
    {
        drogon::app().setMaxConnectionNum(maxConns);
    }
    // max connections per IP
    auto maxConnsPerIP = app.get("max_connections_per_ip", 0).asUInt64();
    if (maxConnsPerIP > 0)
    {
        drogon::app().setMaxConnectionNumPerIP(maxConnsPerIP);
    }
#ifndef _WIN32
    // dynamic views
    auto enableDynamicViews = app.get("load_dynamic_views", false).asBool();
    if (enableDynamicViews)
    {
        auto viewsPaths = app["dynamic_views_path"];
        if (viewsPaths.isArray() && viewsPaths.size() > 0)
        {
            std::vector<std::string> paths;
            for (auto const &viewsPath : viewsPaths)
            {
                paths.push_back(viewsPath.asString());
                LOG_TRACE << "views path:" << paths.back();
            }
            auto outputPath =
                app.get("dynamic_views_output_path", "").asString();
            drogon::app().enableDynamicViewsLoading(paths, outputPath);
        }
    }
#endif
    auto unicodeEscaping =
        app.get("enable_unicode_escaping_in_json", true).asBool();
    drogon::app().setUnicodeEscapingInJson(unicodeEscaping);
    auto &precision = app["float_precision_in_json"];
    if (!precision.isNull())
    {
        auto precisionLength = precision.get("precision", 0).asUInt64();
        auto precisionType =
            precision.get("precision_type", "significant").asString();
        drogon::app().setFloatPrecisionInJson((unsigned int)precisionLength,
                                              precisionType);
    }
    // log
    loadLogSetting(app["log"]);
    // run as daemon
    auto runAsDaemon = app.get("run_as_daemon", false).asBool();
    if (runAsDaemon)
    {
        drogon::app().enableRunAsDaemon();
    }
    // handle SIGTERM
    auto handleSigterm = app.get("handle_sig_term", true).asBool();
    if (!handleSigterm)
    {
        drogon::app().disableSigtermHandling();
    }
    // relaunch
    auto relaunch = app.get("relaunch_on_error", false).asBool();
    if (relaunch)
    {
        drogon::app().enableRelaunchOnError();
    }
    auto useSendfile = app.get("use_sendfile", true).asBool();
    drogon::app().enableSendfile(useSendfile);
    auto useGzip = app.get("use_gzip", true).asBool();
    drogon::app().enableGzip(useGzip);
    auto useBr = app.get("use_brotli", false).asBool();
    drogon::app().enableBrotli(useBr);
    auto staticFilesCacheTime = app.get("static_files_cache_time", 5).asInt();
    drogon::app().setStaticFilesCacheTime(staticFilesCacheTime);
    loadControllers(app["simple_controllers_map"]);
    // Kick off idle connections
    auto kickOffTimeout = app.get("idle_connection_timeout", 60).asUInt64();
    drogon::app().setIdleConnectionTimeout(kickOffTimeout);
    auto server = app.get("server_header_field", "").asString();
    if (!server.empty())
        drogon::app().setServerHeaderField(server);
    auto sendServerHeader = app.get("enable_server_header", true).asBool();
    drogon::app().enableServerHeader(sendServerHeader);
    auto sendDateHeader = app.get("enable_date_header", true).asBool();
    drogon::app().enableDateHeader(sendDateHeader);
    auto keepaliveReqs = app.get("keepalive_requests", 0).asUInt64();
    drogon::app().setKeepaliveRequestsNumber(keepaliveReqs);
    auto pipeliningReqs = app.get("pipelining_requests", 0).asUInt64();
    drogon::app().setPipeliningRequestsNumber(pipeliningReqs);
    auto useGzipStatic = app.get("gzip_static", true).asBool();
    drogon::app().setGzipStatic(useGzipStatic);
    auto useBrStatic = app.get("br_static", true).asBool();
    drogon::app().setBrStatic(useBrStatic);
    auto maxBodySize = app.get("client_max_body_size", "1M").asString();
    size_t size;
    if (bytesSize(maxBodySize, size))
    {
        drogon::app().setClientMaxBodySize(size);
    }
    else
    {
        throw std::runtime_error("Error format of client_max_body_size");
    }
    auto maxMemoryBodySize =
        app.get("client_max_memory_body_size", "64K").asString();
    if (bytesSize(maxMemoryBodySize, size))
    {
        drogon::app().setClientMaxMemoryBodySize(size);
    }
    else
    {
        throw std::runtime_error("Error format of client_max_memory_body_size");
    }
    auto maxWsMsgSize =
        app.get("client_max_websocket_message_size", "128K").asString();
    if (bytesSize(maxWsMsgSize, size))
    {
        drogon::app().setClientMaxWebSocketMessageSize(size);
    }
    else
    {
        throw std::runtime_error(
            "Error format of client_max_websocket_message_size");
    }
    drogon::app().enableReusePort(app.get("reuse_port", false).asBool());
    drogon::app().setHomePage(app.get("home_page", "index.html").asString());
    drogon::app().setImplicitPageEnable(
        app.get("use_implicit_page", true).asBool());
    drogon::app().setImplicitPage(
        app.get("implicit_page", "index.html").asString());
    auto mimes = app["mime"];
    if (!mimes.isNull())
    {
        auto names = mimes.getMemberNames();
        for (const auto &mime : names)
        {
            auto ext = mimes[mime];
            std::vector<std::string> exts;
            if (ext.isString())
                exts.push_back(ext.asString());
            else if (ext.isArray())
            {
                for (const auto &extension : ext)
                    exts.push_back(extension.asString());
            }

            for (const auto &extension : exts)
                drogon::app().registerCustomExtensionMime(extension, mime);
        }
    }
    bool enableCompressedRequests =
        app.get("enabled_compresed_request", false).asBool();
    drogon::app().enableCompressedRequest(enableCompressedRequests);
}
static void loadDbClients(const Json::Value &dbClients)
{
    if (!dbClients)
        return;
    for (auto const &client : dbClients)
    {
        auto type = client.get("rdbms", "postgresql").asString();
        std::transform(type.begin(),
                       type.end(),
                       type.begin(),
                       [](unsigned char c) { return tolower(c); });
        auto host = client.get("host", "127.0.0.1").asString();
        auto port = client.get("port", 5432).asUInt();
        auto dbname = client.get("dbname", "").asString();
        if (dbname == "" && type != "sqlite3")
        {
            throw std::runtime_error(
                "Please configure dbname in the configuration file");
        }
        auto user = client.get("user", "postgres").asString();
        auto password = client.get("passwd", "").asString();
        if (password.empty())
        {
            password = client.get("password", "").asString();
        }
        auto connNum = client.get("connection_number", 1).asUInt();
        if (connNum == 1)
        {
            connNum = client.get("number_of_connections", 1).asUInt();
        }
        auto name = client.get("name", "default").asString();
        auto filename = client.get("filename", "").asString();
        auto isFast = client.get("is_fast", false).asBool();
        auto characterSet = client.get("characterSet", "").asString();
        if (characterSet.empty())
        {
            characterSet = client.get("client_encoding", "").asString();
        }
        auto timeout = client.get("timeout", -1.0).asDouble();
        auto autoBatch = client.get("auto_batch", false).asBool();
        drogon::app().createDbClient(type,
                                     host,
                                     (unsigned short)port,
                                     dbname,
                                     user,
                                     password,
                                     connNum,
                                     filename,
                                     name,
                                     isFast,
                                     characterSet,
                                     timeout,
                                     autoBatch);
    }
}

static void loadRedisClients(const Json::Value &redisClients)
{
    if (!redisClients)
        return;
    for (auto const &client : redisClients)
    {
        std::promise<std::string> promise;
        auto future = promise.get_future();
        auto host = client.get("host", "127.0.0.1").asString();
        trantor::Resolver::newResolver()->resolve(
            host, [&promise](const trantor::InetAddress &address) {
                promise.set_value(address.toIp());
            });
        auto port = client.get("port", 6379).asUInt();
        auto username = client.get("username", "").asString();
        auto password = client.get("passwd", "").asString();
        if (password.empty())
        {
            password = client.get("password", "").asString();
        }
        auto connNum = client.get("connection_number", 1).asUInt();
        if (connNum == 1)
        {
            connNum = client.get("number_of_connections", 1).asUInt();
        }
        auto name = client.get("name", "default").asString();
        auto isFast = client.get("is_fast", false).asBool();
        auto timeout = client.get("timeout", -1.0).asDouble();
        auto db = client.get("db", 0).asUInt();
        auto hostIp = future.get();
        drogon::app().createRedisClient(hostIp,
                                        port,
                                        name,
                                        password,
                                        connNum,
                                        isFast,
                                        timeout,
                                        db,
                                        username);
    }
}

static void loadListeners(const Json::Value &listeners)
{
    if (!listeners)
        return;
    LOG_TRACE << "Has " << listeners.size() << " listeners";
    for (auto const &listener : listeners)
    {
        auto addr = listener.get("address", "0.0.0.0").asString();
        auto port = (uint16_t)listener.get("port", 0).asUInt();
        auto useSSL = listener.get("https", false).asBool();
        auto cert = listener.get("cert", "").asString();
        auto key = listener.get("key", "").asString();
        auto useOldTLS = listener.get("use_old_tls", false).asBool();
        std::vector<std::pair<std::string, std::string>> sslConfCmds;
        if (listener.isMember("ssl_conf"))
        {
            for (const auto &opt : listener["ssl_conf"])
            {
                if (opt.size() == 0 || opt.size() > 2)
                {
                    LOG_FATAL << "SSL configuration option should be an 1 or "
                                 "2-element array";
                    abort();
                }
                sslConfCmds.emplace_back(opt[0].asString(),
                                         opt.get(1, "").asString());
            }
        }
        LOG_TRACE << "Add listener:" << addr << ":" << port;
        drogon::app().addListener(
            addr, port, useSSL, cert, key, useOldTLS, sslConfCmds);
    }
}
static void loadSSL(const Json::Value &sslConf)
{
    if (!sslConf)
        return;
    auto key = sslConf.get("key", "").asString();
    auto cert = sslConf.get("cert", "").asString();
    drogon::app().setSSLFiles(cert, key);
    std::vector<std::pair<std::string, std::string>> sslConfCmds;
    if (sslConf.isMember("conf"))
    {
        for (const auto &opt : sslConf["conf"])
        {
            if (opt.size() == 0 || opt.size() > 2)
            {
                LOG_FATAL << "SSL configuration option should be an 1 or "
                             "2-element array";
                abort();
            }
            sslConfCmds.emplace_back(opt[0].asString(),
                                     opt.get(1, "").asString());
        }
    }
    drogon::app().setSSLConfigCommands(sslConfCmds);
}
void ConfigLoader::load()
{
    // std::cout<<configJsonRoot_<<std::endl;
    loadApp(configJsonRoot_["app"]);
    loadSSL(configJsonRoot_["ssl"]);
    loadListeners(configJsonRoot_["listeners"]);
    loadDbClients(configJsonRoot_["db_clients"]);
    loadRedisClients(configJsonRoot_["redis_clients"]);
}
