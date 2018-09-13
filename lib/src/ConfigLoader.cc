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

#include "ConfigLoader.h"

#include <trantor/utils/Logger.h>
#include <drogon/HttpAppFramework.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
using namespace drogon;

ConfigLoader::ConfigLoader(const std::string &configFile) {
    if(access(configFile.c_str(),0)!=0)
    {
        std::cerr<<"Config file "<<configFile<<" not found!"<<std::endl;
        exit(1);
    }
    if(access(configFile.c_str(),R_OK)!=0)
    {
        std::cerr<<"No permission to read config file "<<configFile<<std::endl;
        exit(1);
    }
    _configFile=configFile;
    std::ifstream infile(configFile.c_str(),std::ifstream::in);
    if(infile)
    {
        try {
            infile>>_configJsonRoot;
        }
        catch (const std::exception &exception)
        {
            std::cerr<<"Configuration file format error! in "<<configFile<<":"<<std::endl;
            std::cerr<<exception.what()<<std::endl;
            exit(1);
        }


    }
}
ConfigLoader::~ConfigLoader() {

}
static void loadLogSetting(const Json::Value &log)
{
    if(!log)
        return;
    auto logPath=log.get("log_path","").asString();
    if(logPath!="")
    {
        auto baseName=log.get("logfile_base_name","").asString();
        auto logSize=log.get("log_size_limit",100000000).asUInt64();
        HttpAppFramework::instance().setLogPath(logPath,baseName,logSize);
    }
}
static void loadApp(const Json::Value &app)
{
    if(!app)
        return;
    //threads number
    auto threadsNum=app.get("threads_num",1).asUInt64();
    if(threadsNum>1)
        HttpAppFramework::instance().setThreadNum(threadsNum);
    //session
    auto enableSession=app.get("enable_session",false).asBool();
    auto timeout=app.get("session_timeout",0).asUInt64();
    if(enableSession)
        HttpAppFramework::instance().enableSession(timeout);
    //document root
    auto documentRoot=app.get("document_root","").asString();
    if(documentRoot!="")
    {
        HttpAppFramework::instance().setDocumentRoot(documentRoot);
    }
    //file types
    auto fileTypes=app["file_types"];
    if(fileTypes&&fileTypes.isArray()&&!fileTypes.empty())
    {
        std::vector<std::string> types;
        for(auto fileType:fileTypes)
        {
            types.push_back(fileType.asString());
            LOG_TRACE<<"file type:"<<types.back();
        }
        HttpAppFramework::instance().setFileTypes(types);
    }
    //max connections
    auto maxConns=app.get("max_connections",0).asUInt64();
    if(maxConns>0)
    {
        HttpAppFramework::instance().setMaxConnectionNum(maxConns);
    }
    //dynamic views
    auto enableDynamicViews=app.get("load_dynamic_views",false).asBool();
    if(enableDynamicViews)
    {
        auto viewsPaths=app["dynamic_views_path"];
        if(viewsPaths&&viewsPaths.isArray()&&viewsPaths.size()>0)
        {
            std::vector<std::string> paths;
            for(auto viewsPath:viewsPaths)
            {
                paths.push_back(viewsPath.asString());
                LOG_TRACE<<"views path:"<<paths.back();
            }
            HttpAppFramework::instance().enableDynamicViewsLoading(paths);
        }
    }
    //log
    loadLogSetting(app["log"]);
    //run as daemon
    auto runAsDaemon=app.get("run_as_daemon",false).asBool();
    if(runAsDaemon)
    {
        HttpAppFramework::instance().enableRunAsDaemon();
    }
    //relaunch
    auto relaunch=app.get("relaunch_on_error",false).asBool();
    if(relaunch)
    {
        HttpAppFramework::instance().enableRelaunchOnError();
    }
}
static void loadListeners(const Json::Value &listeners)
{
    if(!listeners)
        return;
    LOG_TRACE<<"Has "<<listeners.size()<<" listeners";
    for(auto listener:listeners)
    {
        auto addr=listener.get("address","0.0.0.0").asString();
        auto port=(uint16_t)listener.get("port",0).asUInt();
        auto useSSL=listener.get("https",false).asBool();
        auto cert=listener.get("cert","").asString();
        auto key=listener.get("key","").asString();
        LOG_TRACE<<"Add listener:"<<addr<<":"<<port;
        HttpAppFramework::instance().addListener(addr,port,useSSL,cert,key);
    }
}
static void loadSSL(const Json::Value &sslFiles)
{
    if(!sslFiles)
        return;
    auto key=sslFiles.get("key","").asString();
    auto cert=sslFiles.get("cert","").asString();
    HttpAppFramework::instance().setSSLFiles(cert,key);
}
void ConfigLoader::load() {
    std::cout<<_configJsonRoot<<std::endl;
    loadApp(_configJsonRoot["app"]);
    loadSSL(_configJsonRoot["ssl"]);
    loadListeners(_configJsonRoot["listeners"]);
}