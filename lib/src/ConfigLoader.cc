/**
 *
 *  ConfigLoader.cc
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

#include "ConfigLoader.h"
#include <drogon/config.h>
#include <drogon/HttpAppFramework.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <trantor/utils/Logger.h>
#include <unistd.h>

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
    if (access(configFile.c_str(), 0) != 0)
    {
        std::cerr << "Config file " << configFile << " not found!" << std::endl;
        exit(1);
    }
    if (access(configFile.c_str(), R_OK) != 0)
    {
        std::cerr << "No permission to read config file " << configFile
                  << std::endl;
        exit(1);
    }
    _configFile = configFile;
    std::ifstream infile(configFile.c_str(), std::ifstream::in);
    if (infile)
    {
        try
        {
            infile >> _configJsonRoot;
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Configuration file format error! in " << configFile
                      << ":" << std::endl;
            std::cerr << exception.what() << std::endl;
            exit(1);
        }
    }
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
        drogon::app().setLogPath(logPath, baseName, logSize);
    }
    auto logLevel = log.get("log_level", "DEBUG").asString();
    if (logLevel == "TRACE")
    {
        trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    }
    else if (logLevel == "DEBUG")
    {
        trantor::Logger::setLogLevel(trantor::Logger::DEBUG);
    }
    else if (logLevel == "INFO")
    {
        trantor::Logger::setLogLevel(trantor::Logger::INFO);
    }
    else if (logLevel == "WARN")
    {
        trantor::Logger::setLogLevel(trantor::Logger::WARN);
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
                               tolower);
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
    if (enableSession)
        drogon::app().enableSession(timeout);
    else
        drogon::app().disableSession();
    // document root
    auto documentRoot = app.get("document_root", "").asString();
    if (documentRoot != "")
    {
        drogon::app().setDocumentRoot(documentRoot);
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
            drogon::app().enableDynamicViewsLoading(paths);
        }
    }
    // log
    loadLogSetting(app["log"]);
    // run as daemon
    auto runAsDaemon = app.get("run_as_daemon", false).asBool();
    if (runAsDaemon)
    {
        drogon::app().enableRunAsDaemon();
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
    auto maxBodySize = app.get("client_max_body_size", "1M").asString();
    size_t size;
    if (bytesSize(maxBodySize, size))
    {
        drogon::app().setClientMaxBodySize(size);
    }
    else
    {
        std::cerr << "Error format of client_max_body_size" << std::endl;
        exit(1);
    }
    auto maxMemoryBodySize =
        app.get("client_max_memory_body_size", "64K").asString();
    if (bytesSize(maxMemoryBodySize, size))
    {
        drogon::app().setClientMaxMemoryBodySize(size);
    }
    else
    {
        std::cerr << "Error format of client_max_memory_body_size" << std::endl;
        exit(1);
    }
    auto maxWsMsgSize =
        app.get("client_max_websocket_message_size", "128K").asString();
    if (bytesSize(maxWsMsgSize, size))
    {
        drogon::app().setClientMaxWebSocketMessageSize(size);
    }
    else
    {
        std::cerr << "Error format of client_max_websocket_message_size"
                  << std::endl;
        exit(1);
    }
    drogon::app().setHomePage(app.get("home_page", "index.html").asString());
}
static void loadDbClients(const Json::Value &dbClients)
{
    if (!dbClients)
        return;
    for (auto const &client : dbClients)
    {
        auto type = client.get("rdbms", "postgresql").asString();
        auto host = client.get("host", "127.0.0.1").asString();
        auto port = client.get("port", 5432).asUInt();
        auto dbname = client.get("dbname", "").asString();
        if (dbname == "")
        {
            std::cerr << "Please configure dbname in the configuration file"
                      << std::endl;
            exit(1);
        }
        auto user = client.get("user", "postgres").asString();
        auto password = client.get("passwd", "").asString();
        auto connNum = client.get("connection_number", 1).asUInt();
        auto name = client.get("name", "default").asString();
        auto filename = client.get("filename", "").asString();
        auto isFast = client.get("is_fast", false).asBool();
        drogon::app().createDbClient(type,
                                     host,
                                     (u_short)port,
                                     dbname,
                                     user,
                                     password,
                                     connNum,
                                     filename,
                                     name,
                                     isFast);
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
        LOG_TRACE << "Add listener:" << addr << ":" << port;
        drogon::app().addListener(addr, port, useSSL, cert, key);
    }
}
static void loadSSL(const Json::Value &sslFiles)
{
    if (!sslFiles)
        return;
    auto key = sslFiles.get("key", "").asString();
    auto cert = sslFiles.get("cert", "").asString();
    drogon::app().setSSLFiles(cert, key);
}
void ConfigLoader::load()
{
    // std::cout<<_configJsonRoot<<std::endl;
    loadApp(_configJsonRoot["app"]);
    loadSSL(_configJsonRoot["ssl"]);
    loadListeners(_configJsonRoot["listeners"]);
    loadDbClients(_configJsonRoot["db_clients"]);
}
