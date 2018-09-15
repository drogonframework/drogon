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

#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpClientImpl.h"
#include "SharedLibManager.h"
#include "WebSockectConnectionImpl.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpSimpleController.h>
#include <drogon/version.h>

#include <string>
#include <vector>
#include <memory>
#include <regex>

namespace drogon
{
    class HttpAppFrameworkImpl:public HttpAppFramework
    {
    public:
        HttpAppFrameworkImpl():
        _connectionNum(0)
        {
        }
        virtual void addListener(const std::string &ip,
                                 uint16_t port,
                                 bool useSSL=false,
                                 const std::string & certFile="",
                                 const std::string & keyFile="") override;
        virtual void setThreadNum(size_t threadNum) override;
        virtual void setSSLFiles(const std::string &certPath,
                                 const std::string &keyPath) override;
        virtual void run() override ;
        virtual void registerWebSocketController(const std::string &pathName,
                                                 const std::string &crtlName,
                                                 const std::vector<std::string> &filters=
                                                 std::vector<std::string>())override ;
        virtual void registerHttpSimpleController(const std::string &pathName,
                                                  const std::string &crtlName,
                                                  const std::vector<std::string> &filters=
                                                  std::vector<std::string>())override ;
        virtual void registerHttpApiController(const std::string &pathPattern,
                                               const HttpApiBinderBasePtr &binder,
                                               const std::vector<std::string> &filters=std::vector<std::string>()) override ;
        virtual void enableSession(const size_t timeout=0) override { _useSession=true;_sessionTimeout=timeout;}
        virtual void disableSession() override { _useSession=false;}
        virtual const std::string & getDocumentRoot() const override {return _rootPath;}
        virtual void setDocumentRoot(const std::string &rootPath) override {_rootPath=rootPath;}
        virtual void setFileTypes(const std::vector<std::string> &types) override;
        virtual void enableDynamicViewsLoading(const std::vector<std::string> &libPaths) override;
        virtual void setMaxConnectionNum(size_t maxConnections) override;
        virtual void loadConfigFile(const std::string &fileName) override;
        virtual void enableRunAsDaemon() override {_runAsDaemon=true;}
        virtual void enableRelaunchOnError() override {_relaunchOnError=true;}
        virtual void setLogPath(const std::string &logPath,
                                const std::string &logfileBaseName="",
                                size_t logfileSize=100000000) override;
        ~HttpAppFrameworkImpl(){}

        trantor::EventLoop *loop();
    private:
        std::vector<std::tuple<std::string,uint16_t,bool,std::string,std::string>> _listeners;
        void onAsyncRequest(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)> & callback);
        void onNewWebsockRequest(const HttpRequestPtr& req,
                                 const std::function<void (const HttpResponsePtr &)> & callback,
                                 const WebSocketConnectionPtr &wsConnPtr);
        void onWebsockMessage(const WebSocketConnectionPtr &wsConnPtr,trantor::MsgBuffer *buffer);
        void onWebsockDisconnect(const WebSocketConnectionPtr &wsConnPtr);
        void onConnection(const TcpConnectionPtr &conn);
        void readSendFile(const std::string& filePath,const HttpRequestPtr& req,const HttpResponsePtr resp);
        void addApiPath(const std::string &path,
                        const HttpApiBinderBasePtr &binder,
                        const std::vector<std::string> &filters);
        void initRegex();
        //if uuid package found,we can use a uuid string as session id;
        //set _sessionTimeout=0 to make location session valid forever based on cookies;
        size_t _sessionTimeout= 0;
        bool _useSession= false;
        typedef std::shared_ptr<Session> SessionPtr;
        std::unique_ptr<CacheMap<std::string,SessionPtr>> _sessionMapPtr;

        void doFilters(const std::vector<std::string> &filters,
                       const HttpRequestPtr& req,
                       const std::function<void (const HttpResponsePtr &)> & callback,
                       bool needSetJsessionid,
                       const std::string &session_id,
                       const std::function<void ()> &missCallback);
        void doFilterChain(const std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> &chain,
        const HttpRequestPtr& req,
        const std::function<void (const HttpResponsePtr &)> & callback,
        bool needSetJsessionid,
        const std::string &session_id,
        const std::function<void ()> &missCallback);
        //
        struct ControllerAndFiltersName
        {
            std::string controllerName;
            std::vector<std::string> filtersName;
            std::shared_ptr<HttpSimpleControllerBase> controller;
            std::mutex _mutex;
        };
        std::unordered_map<std::string,ControllerAndFiltersName>_simpCtrlMap;
        std::mutex _simpCtrlMutex;
        struct WSCtrlAndFiltersName
        {
            WebSocketControllerBasePtr controller;
            std::vector<std::string> filtersName;
        };
        std::unordered_map<std::string,WSCtrlAndFiltersName> _websockCtrlMap;
        std::mutex _websockCtrlMutex;

        struct ApiBinder
        {
            std::string pathParameterPattern;
            std::vector<size_t> parameterPlaces;
            std::map<std::string,size_t> queryParametersPlaces;
            HttpApiBinderBasePtr binderPtr;
            std::vector<std::string> filtersName;
        };
        //std::unordered_map<std::string,ApiBinder>_apiCtrlMap;
        std::vector<ApiBinder>_apiCtrlVector;
        std::mutex _apiCtrlMutex;

        std::regex _apiRegex;
        bool _enableLastModify=true;
        std::set<std::string> _fileTypeSet={"html","js","css","xml","xsl","txt","svg","ttf",
                                            "otf","woff2","woff","eot","png","jpg","jpeg",
                                            "gif","bmp","ico","icns"};
        std::string _rootPath="./";

        std::atomic_bool _running;

        //tool funcs


        size_t _threadNum=1;
        std::vector<std::string> _libFilePaths;

        std::unique_ptr<SharedLibManager>_sharedLibManagerPtr;

        trantor::EventLoop _loop;

        std::string _sslCertPath;
        std::string _sslKeyPath;

        size_t _maxConnectionNum=100000;
        std::atomic<uint64_t> _connectionNum;

        bool _runAsDaemon=false;
        bool _relaunchOnError=false;
        std::string _logPath="";
        std::string _logfileBaseName="";
        size_t _logfileSize=100000000;
    };
}
