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
#include "HttpAppFrameworkImpl.h"
#include "ConfigLoader.h"
#include "HttpServer.h"
#include <drogon/utils/Utilities.h>
#include <drogon/DrClassMap.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/CacheMap.h>
#include <drogon/Session.h>
#include <trantor/utils/AsyncFileLogger.h>
#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include "Sha1.h"
#endif
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

using namespace drogon;
using namespace std::placeholders;
std::map<std::string,std::shared_ptr<drogon::DrObjectBase>> HttpApiBinderBase::_objMap;
std::mutex HttpApiBinderBase::_objMutex;

static void godaemon(void)
{
    int fs;

    printf("Initializing daemon mode\n");

    if (getppid() != 1)
    {
        fs = fork();

        if (fs > 0)
            exit(0); // parent

        if (fs < 0)
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

    open("/dev/null", O_RDWR);
    int ret = dup(0);
    ret = dup(0);
    (void)ret;
    umask(0);

    return;
}

void HttpAppFrameworkImpl::enableDynamicViewsLoading(const std::vector<std::string> &libPaths)
{
    assert(!_running);

    for(auto libpath:libPaths)
    {
        if(libpath[0]!='/'&&libpath[0]!='.'&&libpath[1]!='/')
        {
	    if(_rootPath[_rootPath.length()-1]=='/')
                _libFilePaths.push_back(_rootPath+libpath);
	    else
                _libFilePaths.push_back(_rootPath+"/"+libpath);
        } else
            _libFilePaths.push_back(libpath);
    }
}
void HttpAppFrameworkImpl::setFileTypes(const std::vector<std::string> &types)
{
    for(auto type : types)
    {
        _fileTypeSet.insert(type);
    }
}
void HttpAppFrameworkImpl::initRegex() {
    std::string regString;
    for(auto &binder:_apiCtrlVector)
    {
        std::regex reg("\\(\\[\\^/\\]\\*\\)");
        std::string tmp=std::regex_replace(binder.pathParameterPattern,reg,"[^/]*");
        regString.append("(").append(tmp).append(")|");
    }
    if(regString.length()>0)
        regString.resize(regString.length()-1);//remove last '|'
    LOG_TRACE<<"regex string:"<<regString;
    _apiRegex=std::regex(regString);
}
void HttpAppFrameworkImpl::registerWebSocketController(const std::string &pathName,
                                                       const std::string &ctrlName,
                                                       const std::vector<std::string> &filters)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(),pathName.end(),path.begin(),tolower);
    auto objPtr=std::shared_ptr<DrObjectBase>(DrClassMap::newObject(ctrlName));
    auto ctrlPtr=std::dynamic_pointer_cast<WebSocketControllerBase>(objPtr);
    assert(ctrlPtr);
    std::lock_guard<std::mutex> guard(_websockCtrlMutex);

    _websockCtrlMap[path].controller=ctrlPtr;
    _websockCtrlMap[path].filtersName=filters;
}
void HttpAppFrameworkImpl::registerHttpSimpleController(const std::string &pathName,
                                                        const std::string &ctrlName,
                                                        const std::vector<std::string> &filters)
{
    assert(!pathName.empty());
    assert(!ctrlName.empty());

    std::string path(pathName);
    std::transform(pathName.begin(),pathName.end(),path.begin(),tolower);
    std::lock_guard<std::mutex> guard(_simpCtrlMutex);
    _simpCtrlMap[path].controllerName=ctrlName;
    _simpCtrlMap[path].filtersName=filters;
}
void HttpAppFrameworkImpl::addApiPath(const std::string &path,
                const HttpApiBinderBasePtr &binder,
                const std::vector<std::string> &filters)
{
    //path will be like /api/v1/service/method/{1}/{2}/xxx...
    std::vector<size_t> places;
    std::string tmpPath=path;
    std::string paras="";
    std::regex regex=std::regex("\\{([0-9]+)\\}");
    std::smatch results;
    auto pos=tmpPath.find("?");
    if(pos!=std::string::npos)
    {
        paras=tmpPath.substr(pos+1);
        tmpPath=tmpPath.substr(0,pos);
    }
    std::string originPath=tmpPath;
    while(std::regex_search(tmpPath,results,regex))
    {
        if(results.size()>1)
        {
            size_t place=(size_t)std::stoi(results[1].str());
            if(place>binder->paramCount()||place==0)
            {
                LOG_ERROR<<"parameter placeholder(value="<<place<<") out of range (1 to "
                                                                  <<binder->paramCount()<<")";
                exit(0);
            }
            places.push_back(place);
        }
        tmpPath=results.suffix();
    }
    std::map<std::string,size_t> parametersPlaces;
    if(!paras.empty())
    {

        std::regex pregex("([^&]*)=\\{([0-9]+)\\}&*");
        while(std::regex_search(paras,results,pregex))
        {
            if(results.size()>2)
            {
                size_t place=(size_t)std::stoi(results[2].str());
                if(place>binder->paramCount()||place==0)
                {
                    LOG_ERROR<<"parameter placeholder(value="<<place<<") out of range (1 to "
                             <<binder->paramCount()<<")";
                    exit(0);
                }
                parametersPlaces[results[1].str()]=place;
            }
            paras=results.suffix();
        }
    }
    struct ApiBinder _binder;
    _binder.parameterPlaces=std::move(places);
    _binder.queryParametersPlaces=std::move(parametersPlaces);
    _binder.binderPtr=binder;
    _binder.filtersName=filters;
    _binder.pathParameterPattern=std::regex_replace(originPath,regex,"([^/]*)");
    std::lock_guard<std::mutex> guard(_apiCtrlMutex);
    _apiCtrlVector.push_back(std::move(_binder));
}
void HttpAppFrameworkImpl::registerHttpApiController(const std::string &pathPattern,
                                       const HttpApiBinderBasePtr &binder,
                                       const std::vector<std::string> &filters)
{
    assert(!pathPattern.empty());
    assert(binder);
    std::string path(pathPattern);

    std::transform(path.begin(),path.end(),path.begin(),tolower);
    addApiPath(path,binder,filters);

}
void HttpAppFrameworkImpl::setThreadNum(size_t threadNum)
{
    assert(threadNum>=1);
    _threadNum=threadNum;
}
void HttpAppFrameworkImpl::setMaxConnectionNum(size_t maxConnections)
{
    _maxConnectionNum=maxConnections;
}
void HttpAppFrameworkImpl::setMaxConnectionNumPerIP(size_t maxConnectionsPerIP)
{
    _maxConnectionNumPerIP=maxConnectionsPerIP;
}
void HttpAppFrameworkImpl::loadConfigFile(const std::string &fileName)
{
    ConfigLoader loader(fileName);
    loader.load();
}
void HttpAppFrameworkImpl::setLogPath(const std::string &logPath,
                                      const std::string &logfileBaseName,
                                      size_t logfileSize)
{
    if(logPath=="")
        return;
    if(access(logPath.c_str(),0)!=0)
    {
        std::cerr<<"Log path dose not exist!\n";
        exit(1);
    }
    if(access(logPath.c_str(),R_OK|W_OK)!=0)
    {
        std::cerr<<"Unable to access log path!\n";
        exit(1);
    }
    _logPath=logPath;
    _logfileBaseName=logfileBaseName;
    _logfileSize=logfileSize;
}
void HttpAppFrameworkImpl::setSSLFiles(const std::string &certPath,
                 const std::string &keyPath)
{
    _sslCertPath=certPath;
    _sslKeyPath=keyPath;
}
void HttpAppFrameworkImpl::addListener(const std::string &ip,
                                       uint16_t port,
                                       bool useSSL,
                                       const std::string & certFile,
                                       const std::string & keyFile)
{
    assert(!_running);

#ifndef USE_OPENSSL
    if(useSSL)
        {
            LOG_ERROR<<"Can't use SSL without OpenSSL found in your system";
        }
#endif

    _listeners.push_back(std::make_tuple(ip,port,useSSL,certFile,keyFile));
}

void HttpAppFrameworkImpl::run()
{
    //
    LOG_INFO<<"Start to run...";
    trantor::AsyncFileLogger asyncFileLogger;

    if(_runAsDaemon)
    {
        //go daemon!
        godaemon();
    }

    //set relaunching
    if(_relaunchOnError)
    {
        while(true)
        {
            int child_status = 0;
            auto child_pid = fork();
            if(child_pid < 0)
            {
                LOG_ERROR<<"fork error";
                abort();
            }
            else if(child_pid == 0)
            {
                //child
                break;
            }
            waitpid(child_pid, &child_status, 0);
            sleep(5);
            LOG_INFO<<"start new process";
        }
    }

    //set logger
    if (!_logPath.empty())
    {
        if (access(_logPath.c_str(), R_OK|W_OK) >= 0)
        {
            std::string baseName=_logfileBaseName;
            if(baseName=="")
            {
                baseName="drogon";
            }
            asyncFileLogger.setFileName(baseName,".log",_logPath);
            asyncFileLogger.startLogging();
            trantor::Logger::setOutputFunction([&](const char *msg,const uint64_t len){
                asyncFileLogger.output(msg,len);
            },[&](){
                asyncFileLogger.flush();
            });
            asyncFileLogger.setFileSizeLimit(_logfileSize);
        }
        else
        {
            LOG_ERROR<<"log file path not exist";
            abort();
        }
    }
    if(_relaunchOnError)
    {
        LOG_INFO<<"Start child process";
    }
    //now start runing!!

    _running=true;

    if(!_libFilePaths.empty())
    {
        _sharedLibManagerPtr=std::unique_ptr<SharedLibManager>(new SharedLibManager(&_loop,_libFilePaths));

    }
    std::vector<std::shared_ptr<HttpServer>> servers;
    std::vector<std::shared_ptr<EventLoopThread>> loopThreads;
    initRegex();
    for(auto listener:_listeners)
    {
        LOG_TRACE<<"thread num="<<_threadNum;
#ifdef __linux__
        for(size_t i=0;i<_threadNum;i++)
        {
            auto loopThreadPtr=std::make_shared<EventLoopThread>();
            loopThreadPtr->run();
            loopThreads.push_back(loopThreadPtr);
            auto serverPtr=std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                    InetAddress(std::get<0>(listener),std::get<1>(listener)),"drogon");
            if(std::get<2>(listener))
            {
                //enable ssl;
#ifdef USE_OPENSSL
                auto cert=std::get<3>(listener);
                auto key=std::get<4>(listener);
                if(cert=="")
                    cert=_sslCertPath;
                if(key=="")
                    key=_sslKeyPath;
                if(cert==""||key=="")
                    {
                        std::cerr<<"You can't use https without cert file or key file"<<std::endl;
                        exit(1);
                    }
                serverPtr->enableSSL(cert,key);
#endif
            }
            serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest,this,_1,_2));
            serverPtr->setConnectionCallback(std::bind(&HttpAppFrameworkImpl::onConnection,this,_1));
            serverPtr->start();
            servers.push_back(serverPtr);
        }
#else
        auto loopThreadPtr=std::make_shared<EventLoopThread>();
        loopThreadPtr->run();
        loopThreads.push_back(loopThreadPtr);
        auto serverPtr=std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                    InetAddress(std::get<0>(listener),std::get<1>(listener)),"drogon");
        if(std::get<2>(listener))
        {
            //enable ssl;
#ifdef USE_OPENSSL
            auto cert=std::get<3>(listener);
            auto key=std::get<4>(listener);
            if(cert=="")
                cert=_sslCertPath;
            if(key=="")
                key=_sslKeyPath;
            if(cert==""||key=="")
            {
                std::cerr<<"You can't use https without cert file or key file"<<std::endl;
                exit(1);
            }
            serverPtr->enableSSL(cert,key);
#endif
        }
        serverPtr->setIoLoopNum(_threadNum);
        serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest,this,_1,_2));
        serverPtr->setNewWebsocketCallback(std::bind(&HttpAppFrameworkImpl::onNewWebsockRequest,this,_1,_2,_3));
        serverPtr->setWebsocketMessageCallback(std::bind(&HttpAppFrameworkImpl::onWebsockMessage,this,_1,_2));
        serverPtr->setDisconnectWebsocketCallback(std::bind(&HttpAppFrameworkImpl::onWebsockDisconnect,this,_1));
        serverPtr->setConnectionCallback(std::bind(&HttpAppFrameworkImpl::onConnection,this,_1));
        serverPtr->start();
        servers.push_back(serverPtr);
#endif
    }
    if(_useSession)
    {
        _sessionMapPtr=std::unique_ptr<CacheMap<std::string,SessionPtr>>(new CacheMap<std::string,SessionPtr>(&_loop));
    }
    _responseCacheMap=std::unique_ptr<CacheMap<std::string,HttpResponsePtr>>
            (new CacheMap<std::string,HttpResponsePtr>(&_loop,1.0,4,50));//Max timeout up to about 70 days;
   _loop.loop();
}

void HttpAppFrameworkImpl::doFilterChain(const std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> &chain,
                                         const HttpRequestPtr& req,
                                         const std::function<void (const HttpResponsePtr &)> & callback,
                                         bool needSetJsessionid,
                                         const std::string &session_id,
                                         const std::function<void ()> &missCallback)
{
    if(chain->size()>0) {
        auto filter = chain->front();
        chain->pop();
        filter->doFilter(req, [=](HttpResponsePtr res) {
            if (needSetJsessionid)
                res->addCookie("JSESSIONID", session_id);
            callback(res);
        }, [=](){
            doFilterChain(chain,req,callback,needSetJsessionid,session_id,missCallback);
        });
    }else{
        missCallback();
    }
}
void HttpAppFrameworkImpl::doFilters(const std::vector<std::string> &filters,
                                     const HttpRequestPtr& req,
                                     const std::function<void (const HttpResponsePtr &)> & callback,
                                     bool needSetJsessionid,
                                     const std::string &session_id,
                                     const std::function<void ()> &missCallback)
{
    LOG_TRACE<<"filters count:"<<filters.size();
    std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> filterPtrs=
            std::make_shared<std::queue<std::shared_ptr<HttpFilterBase>>>();
    for(auto filter:filters) {
        auto _object = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(filter));
        auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
        if(_filter)
            filterPtrs->push(_filter);
        else {
            LOG_ERROR<<"filter "<<filter<<" not found";
        }
    }
    doFilterChain(filterPtrs,req,callback,needSetJsessionid,session_id,missCallback);
}
void HttpAppFrameworkImpl::onWebsockDisconnect(const WebSocketConnectionPtr &wsConnPtr)
{
    auto wsConnImplPtr=std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
    assert(wsConnImplPtr);
    auto ctrl=wsConnImplPtr->controller();
    if(ctrl)
    {
        ctrl->handleConnectionClosed(wsConnPtr);
        wsConnImplPtr->setController(WebSocketControllerBasePtr());
    }

}
void HttpAppFrameworkImpl::onConnection(const TcpConnectionPtr &conn)
{
    if(conn->connected())
    {
        if(_connectionNum.fetch_add(1)>=_maxConnectionNum)
        {
            LOG_ERROR<<"too much connections!force close!";
            conn->forceClose();
        }
        else if(_maxConnectionNumPerIP>0)
        {
            {
                auto iter=_connectionsNumMap.find(conn->peerAddr().toIp());
                if(iter==_connectionsNumMap.end())
                {
                    _connectionsNumMap[conn->peerAddr().toIp()]=0;
                }
                if(_connectionsNumMap[conn->peerAddr().toIp()]++>=_maxConnectionNumPerIP)
                {
                    conn->forceClose();
                }
            }
        }
    }
    else
    {
        _connectionNum--;

        if(_maxConnectionNumPerIP>0&&_connectionsNumMap.find(conn->peerAddr().toIp())!=_connectionsNumMap.end())
        {
            std::lock_guard<std::mutex> guard(_connectionsNumMapMutex);
            _connectionsNumMap[conn->peerAddr().toIp()]--;
        }
    }
}
std::string parseWebsockFrame(trantor::MsgBuffer *buffer)
{
    if(buffer->readableBytes()>=2)
    {
        auto secondByte=(*buffer)[1];
        size_t length=secondByte & 127;
        int isMasked=(secondByte & 0x80);
        if(isMasked!=0)
        {
            LOG_TRACE<<"data encoded!";
        } else
            LOG_TRACE<<"plain data";
        size_t indexFirstMask = 2;

        if (length == 126)
        {
            indexFirstMask = 4;
        }
        else if (length == 127)
        {
            indexFirstMask = 10;
        }
        if(indexFirstMask>2&&buffer->readableBytes()>=indexFirstMask)
        {
            if(indexFirstMask==4)
            {
                length=(unsigned char)(*buffer)[2];
                length=(length<<8)+(unsigned char)(*buffer)[3];
                LOG_TRACE<<"bytes[2]="<<(unsigned char)(*buffer)[2];
                LOG_TRACE<<"bytes[3]="<<(unsigned char)(*buffer)[3];
            } else if(indexFirstMask==10)
            {
                length=(unsigned char)(*buffer)[2];
                length=(length<<8)+(unsigned char)(*buffer)[3];
                length=(length<<8)+(unsigned char)(*buffer)[4];
                length=(length<<8)+(unsigned char)(*buffer)[5];
                length=(length<<8)+(unsigned char)(*buffer)[6];
                length=(length<<8)+(unsigned char)(*buffer)[7];
                length=(length<<8)+(unsigned char)(*buffer)[8];
                length=(length<<8)+(unsigned char)(*buffer)[9];
//                length=*((uint64_t *)(buffer->peek()+2));
//                length=ntohll(length);
            } else{
                assert(0);
            }
        }
        LOG_TRACE<<"websocket message len="<<length;
        if(buffer->readableBytes()>=(indexFirstMask+4+length))
        {
            auto masks=buffer->peek()+indexFirstMask;
            int indexFirstDataByte = indexFirstMask + 4;
            auto rawData=buffer->peek()+indexFirstDataByte;
            std::string message;
            message.resize(length);
            LOG_TRACE<<"rawData[0]="<<(unsigned char)rawData[0];
            LOG_TRACE<<"masks[0]="<<(unsigned char)masks[0];
            for(size_t i=0;i<length;i++)
            {
                message[i]=(rawData[i]^masks[i%4]);
            }
            buffer->retrieve(indexFirstMask+4+length);
            LOG_TRACE<<"got message len="<<message.length();
            return message;
        }
    }
    return std::string();
}
void HttpAppFrameworkImpl::onWebsockMessage(const WebSocketConnectionPtr &wsConnPtr,
                                            trantor::MsgBuffer *buffer)
{
    auto wsConnImplPtr=std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
    assert(wsConnImplPtr);
    auto ctrl=wsConnImplPtr->controller();
    if(ctrl)
    {
        std::string message;
        while(!(message=parseWebsockFrame(buffer)).empty())
        {
            LOG_TRACE<<"Got websock message:"<<message;
            ctrl->handleNewMessage(wsConnPtr,std::move(message));
        }
    }
}
void HttpAppFrameworkImpl::onNewWebsockRequest(const HttpRequestPtr& req,
                                               const std::function<void (const HttpResponsePtr &)> & callback,
                                               const WebSocketConnectionPtr &wsConnPtr)
{
    std::string wsKey=req->getHeader("Sec-WebSocket-Key");
    if(!wsKey.empty())
    {
        // magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        WebSocketControllerBasePtr ctrlPtr;
        std::vector<std::string> filtersName;
        {
            std::string pathLower(req->path());
            std::transform(pathLower.begin(),pathLower.end(),pathLower.begin(),tolower);
            std::lock_guard<std::mutex> guard(_websockCtrlMutex);
            if(_websockCtrlMap.find(pathLower)!=_websockCtrlMap.end())
            {
                ctrlPtr=_websockCtrlMap[pathLower].controller;
                filtersName=_websockCtrlMap[pathLower].filtersName;
            }
        }
        if(ctrlPtr)
        {
            doFilters(filtersName,req,callback,false,"",[=]() mutable
            {
                wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
                unsigned char accKey[SHA_DIGEST_LENGTH];
                SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()), wsKey.length(), accKey);
                auto base64Key=base64Encode(accKey,SHA_DIGEST_LENGTH);
                auto resp=HttpResponse::newHttpResponse();
                resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
                resp->addHeader("Upgrade","websocket");
                resp->addHeader("Connection","Upgrade");
                resp->addHeader("Sec-WebSocket-Accept",base64Key);
                callback(resp);
                auto wsConnImplPtr=std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
                assert(wsConnImplPtr);
                wsConnImplPtr->setController(ctrlPtr);
                ctrlPtr->handleNewConnection(req,wsConnPtr);
                return;
            });
            return;
        }
    }
    auto resp=drogon::HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);

}
void HttpAppFrameworkImpl::onAsyncRequest(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)> & callback)
{
    LOG_TRACE << "new request:"<<req->peerAddr().toIpPort()<<"->"<<req->localAddr().toIpPort();
    LOG_TRACE << "Headers " << req->methodString() << " " << req->path();

#if 0
    const std::map<std::string, std::string>& headers = req->headers();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end();
         ++it) {
        LOG_TRACE << it->first << ": " << it->second;
    }

    LOG_TRACE<<"cookies:";
    auto cookies = req->cookies();
    for(auto it=cookies.begin();it!=cookies.end();++it)
    {
        LOG_TRACE<<it->first<<"="<<it->second;
    }
#endif


    LOG_TRACE << "http path=" << req->path();
   // LOG_TRACE << "query: " << req->query() ;

    std::string session_id=req->getCookie("JSESSIONID");
    bool needSetJsessionid=false;
    if(_useSession)
    {
        if(session_id=="")
        {
            session_id=getuuid().c_str();
            needSetJsessionid=true;
            _sessionMapPtr->insert(session_id,std::make_shared< Session >(),_sessionTimeout);
        }
        else
        {
            if(_sessionMapPtr->find(session_id)==false)
            {
                _sessionMapPtr->insert(session_id,std::make_shared< Session >(),_sessionTimeout);
            }
        }
        (std::dynamic_pointer_cast<HttpRequestImpl>(req))->setSession((*_sessionMapPtr)[session_id]);
    }

    std::string path = req->path();
    auto pos = path.rfind(".");
    if(pos != std::string::npos) {
        std::string filetype = path.substr(pos + 1);
        transform(filetype.begin(), filetype.end(), filetype.begin(), tolower);
        if(_fileTypeSet.find(filetype) != _fileTypeSet.end()) {
            LOG_INFO << "file query!";
            std::string filePath = _rootPath + path;
            std::shared_ptr<HttpResponseImpl> resp=std::make_shared<HttpResponseImpl>();

            if(needSetJsessionid)
                resp->addCookie("JSESSIONID",session_id);

            // pick a Content-Type for the file
            if(filetype=="html")	resp->setContentTypeCode(CT_TEXT_HTML);
            else if(filetype=="js")  resp->setContentTypeCode(CT_APPLICATION_X_JAVASCRIPT);
            else if(filetype=="css")  resp->setContentTypeCode(CT_TEXT_CSS);
            else if(filetype=="xml")  resp->setContentTypeCode(CT_TEXT_XML);
            else if(filetype=="xsl") resp->setContentTypeCode(CT_TEXT_XSL);
            else if(filetype=="txt")  resp->setContentTypeCode(CT_TEXT_PLAIN);
            else if(filetype=="svg")  resp->setContentTypeCode(CT_IMAGE_SVG_XML);
            else if(filetype=="ttf")  resp->setContentTypeCode(CT_APPLICATION_X_FONT_TRUETYPE);
            else if(filetype=="otf")  resp->setContentTypeCode(CT_APPLICATION_X_FONT_OPENTYPE);
            else if(filetype=="woff2")resp->setContentTypeCode(CT_APPLICATION_FONT_WOFF2);
            else if(filetype=="woff") resp->setContentTypeCode(CT_APPLICATION_FONT_WOFF);
            else if(filetype=="eot")  resp->setContentTypeCode(CT_APPLICATION_VND_MS_FONTOBJ);
            else if(filetype=="png")  resp->setContentTypeCode(CT_IMAGE_PNG);
            else if(filetype=="jpg")  resp->setContentTypeCode(CT_IMAGE_JPG);
            else if(filetype=="jpeg") resp->setContentTypeCode(CT_IMAGE_JPG);
            else if(filetype=="gif")  resp->setContentTypeCode(CT_IMAGE_GIF);
            else if(filetype=="bmp")  resp->setContentTypeCode(CT_IMAGE_BMP);
            else if(filetype=="ico")  resp->setContentTypeCode(CT_IMAGE_XICON);
            else if(filetype=="icns") resp->setContentTypeCode(CT_IMAGE_ICNS);
            else resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);

            readSendFile(filePath,req, resp);
            callback(resp);
            return;
        }

    }


    /*find simple controller*/
    std::string pathLower(req->path());
    std::transform(pathLower.begin(),pathLower.end(),pathLower.begin(),tolower);

    if(_simpCtrlMap.find(pathLower)!=_simpCtrlMap.end())
    {
        auto &filters=_simpCtrlMap[pathLower].filtersName;
        doFilters(filters,req,callback,needSetJsessionid,session_id,[=](){
	    auto &ctrlItem=_simpCtrlMap[pathLower];
            const std::string &ctrlName = ctrlItem.controllerName;
            std::shared_ptr<HttpSimpleControllerBase> controller;
            HttpResponsePtr responsePtr;
            {
                //maybe update controller,so we use lock_guard to protect;
                std::lock_guard<std::mutex> guard(ctrlItem._mutex);
                controller=ctrlItem.controller;
                responsePtr=ctrlItem.responsePtr.lock();
                if(!controller)
                {
                    auto _object = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(ctrlName));
                    controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
                    _simpCtrlMap[pathLower].controller=controller;
                }
            }


            if(controller) {
                if(responsePtr)
                {
                    //use cached response!
                    LOG_TRACE<<"Use cached response";
                    if(!needSetJsessionid)
                        callback(responsePtr);
                    else
                    {
                        auto newResp=std::make_shared<HttpResponseImpl>
                                (*std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                        newResp->addCookie("JSESSIONID",session_id);
                        newResp->setExpiredTime(-1);
                        callback(newResp);
                    }
                    return;
                }
                else
                {
                    controller->asyncHandleHttpRequest(req, [=](const HttpResponsePtr& resp){
                        if(needSetJsessionid)
                            resp->addCookie("JSESSIONID",session_id);
                        callback(resp);
                        if(resp->expiredTime()>=0)
                        {
                            //cache the response;
                            if(needSetJsessionid)
                            {
                                resp->removeCookie("JSESSIONID");
                                std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                            }
                            std::lock_guard<std::mutex> guard(_simpCtrlMap[pathLower]._mutex);
                            _responseCacheMap->insert(pathLower,resp,resp->expiredTime());
                            _simpCtrlMap[pathLower].responsePtr=resp;

                        }
                    });
                }

                return;
            } else {

                LOG_ERROR << "can't find controller " << ctrlName;
            }
        });
        return;
    }
    //find api controller
    if(_apiRegex.mark_count()>0)
    {
        std::smatch result;
        if(std::regex_match(req->path(),result,_apiRegex))
        {
            for(size_t i=1;i<result.size();i++)
            {
                if(result[i].str().empty())
                    continue;
                if(result[i].str()==req->path()&&i<=_apiCtrlVector.size())
                {
                    size_t ctlIndex=i-1;
                    auto &binder=_apiCtrlVector[ctlIndex];
                    LOG_TRACE<<"got api access,regex="<<binder.pathParameterPattern;
                    auto &filters=binder.filtersName;
                    doFilters(filters,req,callback,needSetJsessionid,session_id,[=](){

                        auto &binder=_apiCtrlVector[ctlIndex];

                        HttpResponsePtr responsePtr;
                        {
                            std::lock_guard<std::mutex> guard(*(binder.binderMtx));
                            responsePtr=binder.responsePtr.lock();
                        }
                        if(responsePtr)
                        {
                            //use cached response!
                            LOG_TRACE<<"Use cached response";
                            if(!needSetJsessionid)
                                callback(responsePtr);
                            else
                            {
                                auto newResp=std::make_shared<HttpResponseImpl>
                                        (*std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                                newResp->addCookie("JSESSIONID",session_id);
                                newResp->setExpiredTime(-1);
                                callback(newResp);
                            }
                            return;
                        }

                        std::vector<std::string> params(binder.parameterPlaces.size());
                        std::smatch r;
                        if(std::regex_match(req->path(),r,std::regex(binder.pathParameterPattern)))
                        {
                            for(size_t j=1;j<r.size();j++)
                            {
                                size_t place=binder.parameterPlaces[j-1];
                                if(place>params.size())
                                    params.resize(place);
                                params[place-1]=r[j].str();
                                LOG_TRACE<<"place="<<place<<" para:"<<params[place-1];
                            }
                        }
                        if(binder.queryParametersPlaces.size()>0)
                        {
                            auto qureyPara=req->getParameters();
                            for(auto parameter:qureyPara)
                            {
                                if(binder.queryParametersPlaces.find(parameter.first)!=
                                   binder.queryParametersPlaces.end())
                                {
                                    auto place=binder.queryParametersPlaces.find(parameter.first)->second;
                                    if(place>params.size())
                                        params.resize(place);
                                    params[place-1]=parameter.second;
                                }
                            }
                        }
                        std::list<std::string> paraList;
                        for(auto p:params)
                        {
                            LOG_TRACE<<p;
                            paraList.push_back(std::move(p));
                        }

                        binder.binderPtr->handleHttpApiRequest(paraList,req,[=](const HttpResponsePtr& resp){
                            LOG_TRACE<<"api resp:needSetJsessionid="<<needSetJsessionid<<";JSESSIONID="<<session_id;
                            if(needSetJsessionid)
                                resp->addCookie("JSESSIONID",session_id);
                            callback(resp);
                            if(resp->expiredTime()>=0)
                            {
                                //cache the response;
                                if(needSetJsessionid)
                                {
                                    resp->removeCookie("JSESSIONID");
                                    std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                                }
                                std::lock_guard<std::mutex> guard(*(_apiCtrlVector[ctlIndex].binderMtx));
                                _responseCacheMap->insert(_apiCtrlVector[ctlIndex].pathParameterPattern,resp,resp->expiredTime());
                                _apiCtrlVector[ctlIndex].responsePtr=resp;

                            }
                        });
                        return;
                    });

                }
            }
        }
        else{
            //No controller found
            auto res=drogon::HttpResponse::newNotFoundResponse();
            if(needSetJsessionid)
                res->addCookie("JSESSIONID",session_id);

            callback(res);
        }
    }
    else{
        //No controller found
        auto res=drogon::HttpResponse::newNotFoundResponse();

        if(needSetJsessionid)
            res->addCookie("JSESSIONID",session_id);

        callback(res);
    }
}

void HttpAppFrameworkImpl::readSendFile(const std::string& filePath,const HttpRequestPtr& req, const HttpResponsePtr resp)
{
//If-Modified-Since: Wed Jun 15 08:57:30 2016 GMT
    std::ifstream infile(filePath, std::ifstream::binary);
    LOG_INFO << "send http file:" << filePath;
    if(!infile) {

        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setCloseConnection(true);
        return;
    }

    if(_enableLastModify)
    {
        struct stat fileStat;
        LOG_TRACE<<"enabled LastModify";
        if(stat(filePath.c_str(),&fileStat)>=0)
        {
            LOG_TRACE<<"last modify time:"<<fileStat.st_mtime;
            struct tm tm1;
            gmtime_r(&fileStat.st_mtime,&tm1);
            char timeBuf[64];
            asctime_r(&tm1,timeBuf);
            std::string timeStr(timeBuf);
            std::string::size_type len=timeStr.length();
            std::string lastModified =timeStr.substr(0,len-1)+" GMT";

            std::string modiStr=req->getHeader("If-Modified-Since");
            if(modiStr!=""&&modiStr==lastModified)
            {
                LOG_TRACE<<"not Modified!";
                resp->setStatusCode(HttpResponse::k304NotModified);
                return;
            }
            resp->addHeader("Last-Modified",lastModified);

            resp->addHeader("Expires","Thu, 01 Jan 1970 00:00:00 GMT");
        }
    }

    if(_useSendfile&&
       (req->getHeader("Accept-Encoding").find("gzip")==std::string::npos||
        resp->getContentTypeCode()>=CT_APPLICATION_OCTET_STREAM))
    {
        //binary file or no gzip supported by client
        std::dynamic_pointer_cast<HttpResponseImpl>(resp)->setSendfile(filePath);
    }
    else
    {
        std::streambuf * pbuf = infile.rdbuf();
        std::streamsize size = pbuf->pubseekoff(0,infile.end);
        pbuf->pubseekoff(0,infile.beg);       // rewind
        std::string str;
        str.resize(size);
        pbuf->sgetn (&str[0],size);
        LOG_INFO << "file len:" << str.length();
        resp->setBody(std::move(str));
    }

    resp->setStatusCode(HttpResponse::k200OK);
}

trantor::EventLoop *HttpAppFrameworkImpl::loop()
{
    return &_loop;
}

HttpAppFramework& HttpAppFramework::instance() {
    static HttpAppFrameworkImpl _instance;
    return _instance;
}

HttpAppFramework::~HttpAppFramework()
{

}




