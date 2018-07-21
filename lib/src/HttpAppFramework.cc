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
#include "SharedLibManager.h"
#include "Utilities.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <drogon/DrClassMap.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/CacheMap.h>
#include <drogon/Session.h>
#include "HttpServer.h"
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <uuid/uuid.h>
#include <unordered_map>
#include <algorithm>
#include <drogon/HttpSimpleController.h>
#include <drogon/version.h>
#include <memory>
#include <regex>
#include <tuple>
#include <utility>

namespace drogon
{

    class HttpAppFrameworkImpl:public HttpAppFramework
    {
    public:
        virtual void addListener(const std::string &ip,uint16_t port,bool useSSL=false) override;
        virtual void setThreadNum(size_t threadNum) override;
        virtual void setSSLFiles(const std::string &certPath,
                                 const std::string &keyPath) override;
        virtual void run() override ;
        virtual void registerHttpSimpleController(const std::string &pathName,const std::string &crtlName,const std::vector<std::string> &filters=
        std::vector<std::string>())override ;
        virtual void registerHttpApiController(const std::string &pathPattern,
                                           const HttpApiBinderBasePtr &binder,
                                           const std::vector<std::string> &filters=std::vector<std::string>()) override ;
        virtual void enableSession(const size_t timeout) override { _useSession=true;_sessionTimeout=timeout;}
        virtual void disableSession() override { _useSession=false;}
        virtual const std::string & getDocumentRoot() const override {return _rootPath;}
        virtual void setDocumentRoot(const std::string &rootPath) override {_rootPath=rootPath;}
        virtual void setFileTypes(const std::vector<std::string> &types) override;
        virtual void enableDynamicSharedLibLoading(const std::vector<std::string> &libPaths) override;
        ~HttpAppFrameworkImpl(){}
    private:
        std::vector<std::tuple<std::string,uint16_t,bool>> _listeners;
        void onAsyncRequest(const HttpRequest& req,const std::function<void (HttpResponse &)> & callback);
        void readSendFile(const std::string& filePath,const HttpRequest& req, HttpResponse* resp);
        void addApiPath(const std::string &path,
                        const HttpApiBinderBasePtr &binder,
                        const std::vector<std::string> &filters);
        void initRegex();
        //if uuid package found,we can use a uuid string as session id;
        //set _sessionTimeout=0 to make location session valid forever based on cookies;
        size_t _sessionTimeout=1200;
        bool _useSession=true;
        typedef std::shared_ptr<Session> SessionPtr;
        std::unique_ptr<CacheMap<std::string,SessionPtr>> _sessionMapPtr;

        bool doFilters(const std::vector<std::string> &filters,
                       const HttpRequest& req,
                       const std::function<void (HttpResponse &)> & callback,
                       bool needSetJsessionid,
                       const std::string &session_id);
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
        std::set<std::string> _fileTypeSet={"html","jpg"};
        std::string _rootPath=".";

        std::atomic_bool _running;

        //tool funcs

        std::string getUuid();
        std::string stringToHex(unsigned char* ptr, long long length);


        size_t _threadNum=1;
        std::vector<std::string> _libFilePaths;

        std::unique_ptr<SharedLibManager>_sharedLibManagerPtr;

        trantor::EventLoop _loop;

        std::string _sslCertPath;
        std::string _sslKeyPath;
    };
}

using namespace drogon;
using namespace std::placeholders;
void HttpAppFrameworkImpl::enableDynamicSharedLibLoading(const std::vector<std::string> &libPaths)
{
    assert(!_running);
    if(_libFilePaths.empty())
    {
        for(auto libpath:libPaths)
        {
            if(libpath[0]!='/')
            {
                _libFilePaths.push_back(_rootPath+"/"+libpath);
            } else
                _libFilePaths.push_back(libpath);
        }

        _sharedLibManagerPtr=std::unique_ptr<SharedLibManager>(new SharedLibManager(&_loop,_libFilePaths));
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
    for(auto binder:_apiCtrlVector)
    {
        std::regex reg("\\(\\[\\^/\\]\\*\\)");
        std::string tmp=std::regex_replace(binder.pathParameterPattern,reg,"[^/]*");
        regString.append("(").append(tmp).append(")|");
    }
    if(regString.length()>0)
        regString.resize(regString.length()-1);//remove last '|'
    LOG_DEBUG<<"regex string:"<<regString;
    _apiRegex=std::regex(regString);
}
void HttpAppFrameworkImpl::registerHttpSimpleController(const std::string &pathName,const std::string &crtlName,const std::vector<std::string> &filters)
{
    assert(!pathName.empty());
    assert(!crtlName.empty());

    std::string path(pathName);
    std::transform(pathName.begin(),pathName.end(),path.begin(),tolower);
    std::lock_guard<std::mutex> guard(_simpCtrlMutex);
    _simpCtrlMap[path].controllerName=crtlName;
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
void HttpAppFrameworkImpl::setSSLFiles(const std::string &certPath,
                 const std::string &keyPath)
{
    _sslCertPath=certPath;
    _sslKeyPath=keyPath;
}
void HttpAppFrameworkImpl::addListener(const std::string &ip, uint16_t port,bool useSSL)
{
    assert(!_running);

#ifndef USE_OPENSSL
    assert(!useSSL);
#endif

    _listeners.push_back(std::make_tuple(ip,port,useSSL));
}

void HttpAppFrameworkImpl::run()
{
    //
    _running=true;
    std::vector<std::shared_ptr<HttpServer>> servers;
    std::vector<std::shared_ptr<EventLoopThread>> loopThreads;
    initRegex();
    for(auto listener:_listeners)
    {
        LOG_DEBUG<<"thread num="<<_threadNum;
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
                assert(!_sslCertPath.empty());
                assert(!_sslKeyPath.empty());
                serverPtr->enableSSL(_sslCertPath,_sslKeyPath);
#endif
            }
            serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest,this,_1,_2));
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
            assert(!_sslCertPath.empty());
            assert(!_sslKeyPath.empty());
            serverPtr->enableSSL(_sslCertPath,_sslKeyPath);
#endif
        }
        serverPtr->setIoLoopNum(_threadNum);
        serverPtr->setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest,this,_1,_2));
        serverPtr->start();
        servers.push_back(serverPtr);
#endif
    }
    int interval,limit;
    if(_sessionTimeout==0)
    {
        interval=1;
        limit=1200;
    }else if(_sessionTimeout<1000)
    {
        interval=1;
        limit=_sessionTimeout;
    } else
    {
        interval=_sessionTimeout/1000;
        limit=_sessionTimeout;
    }

    _sessionMapPtr=std::unique_ptr<CacheMap<std::string,SessionPtr>>(new CacheMap<std::string,SessionPtr>(&_loop,interval,limit));
    _loop.loop();
}

bool HttpAppFrameworkImpl::doFilters(const std::vector<std::string> &filters,
                                     const HttpRequest& req,
                                     const std::function<void (HttpResponse &)> & callback,
                                     bool needSetJsessionid,
                                     const std::string &session_id)
{
    LOG_TRACE<<"filters count:"<<filters.size();
    for(auto filter:filters)
    {
        auto _object=std::shared_ptr<DrObjectBase>(DrClassMap::newObject(filter));
        auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
        if(_filter)
        {
            auto resPtr=_filter->doFilter(req);
            if(resPtr)
            {
                if(needSetJsessionid)
                    resPtr->addCookie("JSESSIONID",session_id);
                callback(*resPtr);
                return true;
            }
        } else
        {
            LOG_ERROR<<"filter "<<filter<<" not found";
        }
    }
    return false;
}

void HttpAppFrameworkImpl::onAsyncRequest(const HttpRequest& req,const std::function<void (HttpResponse &)> & callback)
{
    LOG_TRACE << "new request:"<<req.peerAddr().toIpPort()<<"->"<<req.localAddr().toIpPort();
    LOG_TRACE << "Headers " << req.methodString() << " " << req.path();

#if 1
    const std::map<std::string, std::string>& headers = req.headers();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end();
         ++it) {
        LOG_TRACE << it->first << ": " << it->second;
    }

    LOG_TRACE<<"cookies:";
    auto cookies = req.cookies();
    for(auto it=cookies.begin();it!=cookies.end();++it)
    {
        LOG_TRACE<<it->first<<"="<<it->second;
    }
#endif


    LOG_TRACE << "http path=" << req.path();
    LOG_TRACE << "query: " << req.query() ;

    std::string session_id=req.getCookie("JSESSIONID");
    bool needSetJsessionid=false;
    if(_useSession)
    {
        if(session_id=="")
        {
            session_id=getUuid().c_str();
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
        ((HttpRequestImpl &)req).setSession((*_sessionMapPtr)[session_id]);
    }

    std::string path = req.path();
    auto pos = path.rfind(".");
    if(pos != std::string::npos) {
        std::string filetype = path.substr(pos + 1);
        transform(filetype.begin(), filetype.end(), filetype.begin(), tolower);
        if(_fileTypeSet.find(filetype) != _fileTypeSet.end()) {
            LOG_INFO << "file query!";
            std::string filePath = _rootPath + path;
            HttpResponseImpl resp;

            if(needSetJsessionid)
                resp.addCookie("JSESSIONID",session_id);

            // pick a Content-Type for the file
            if(filetype=="html")	resp.setContentTypeCode(CT_TEXT_HTML);
            else if(filetype=="js")  resp.setContentTypeCode(CT_APPLICATION_X_JAVASCRIPT);
            else if(filetype=="css")  resp.setContentTypeCode(CT_TEXT_CSS);
            else if(filetype=="xml")  resp.setContentTypeCode(CT_TEXT_XML);
            else if(filetype=="xsl") resp.setContentTypeCode(CT_TEXT_XSL);
            else if(filetype=="txt")  resp.setContentTypeCode(CT_TEXT_PLAIN);
            else if(filetype=="svg")  resp.setContentTypeCode(CT_IMAGE_SVG_XML);
            else if(filetype=="ttf")  resp.setContentTypeCode(CT_APPLICATION_X_FONT_TRUETYPE);
            else if(filetype=="otf")  resp.setContentTypeCode(CT_APPLICATION_X_FONT_OPENTYPE);
            else if(filetype=="woff2")resp.setContentTypeCode(CT_APPLICATION_FONT_WOFF2);
            else if(filetype=="woff") resp.setContentTypeCode(CT_APPLICATION_FONT_WOFF);
            else if(filetype=="eot")  resp.setContentTypeCode(CT_APPLICATION_VND_MS_FONTOBJ);
            else if(filetype=="png")  resp.setContentTypeCode(CT_IMAGE_PNG);
            else if(filetype=="jpg")  resp.setContentTypeCode(CT_IMAGE_JPG);
            else if(filetype=="jpeg") resp.setContentTypeCode(CT_IMAGE_JPG);
            else if(filetype=="gif")  resp.setContentTypeCode(CT_IMAGE_GIF);
            else if(filetype=="bmp")  resp.setContentTypeCode(CT_IMAGE_BMP);
            else if(filetype=="ico")  resp.setContentTypeCode(CT_IMAGE_XICON);
            else if(filetype=="icns") resp.setContentTypeCode(CT_IMAGE_ICNS);
            else resp.setContentTypeCode(CT_APPLICATION_OCTET_STREAM);

            readSendFile(filePath,req, &resp);
            callback(resp);
            return;
        }

    }


    /*find simple controller*/
    std::string pathLower(req.path());
    std::transform(pathLower.begin(),pathLower.end(),pathLower.begin(),tolower);

    if(_simpCtrlMap.find(pathLower)!=_simpCtrlMap.end())
    {
        auto &filters=_simpCtrlMap[pathLower].filtersName;
        if(doFilters(filters,req,callback,needSetJsessionid,session_id))
            return;
        const std::string &ctrlName = _simpCtrlMap[pathLower].controllerName;
        std::shared_ptr<HttpSimpleControllerBase> controller;
        {
            //maybe update controller,so we use lock_guard to protect;
            std::lock_guard<std::mutex> guard(_simpCtrlMap[pathLower]._mutex);
            controller=_simpCtrlMap[pathLower].controller;
            if(!controller)
            {
                auto _object = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(ctrlName));
                controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
                _simpCtrlMap[pathLower].controller=controller;
            }
        }


        if(controller) {
            controller->asyncHandleHttpRequest(req, [=](HttpResponse& resp){
                if(needSetJsessionid)
                    resp.addCookie("JSESSIONID",session_id);
                callback(resp);
            });
            return;
        } else {

            LOG_ERROR << "can't find controller " << ctrlName;
        }
    }
    //find api controller
    if(_apiRegex.mark_count()>0)
    {
        std::smatch result;
        if(std::regex_match(req.path(),result,_apiRegex))
        {
            for(size_t i=1;i<result.size();i++)
            {
                if(result[i].str().empty())
                    continue;
                if(result[i].str()==req.path()&&i<=_apiCtrlVector.size())
                {
                    size_t ctlIndex=i-1;
                    auto &binder=_apiCtrlVector[ctlIndex];
                    LOG_DEBUG<<"got api access,regex="<<binder.pathParameterPattern;
                    auto &filters=binder.filtersName;
                    if(doFilters(filters,req,callback,needSetJsessionid,session_id))
                        return;
                    std::vector<std::string> params(binder.parameterPlaces.size());
                    std::smatch r;
                    if(std::regex_match(req.path(),r,std::regex(binder.pathParameterPattern)))
                    {
                        for(size_t j=1;j<r.size();j++)
                        {
                            size_t place=binder.parameterPlaces[j-1];
                            if(place>params.size())
                                params.resize(place);
                            params[place-1]=r[j].str();
                            LOG_DEBUG<<"place="<<place<<" para:"<<params[place-1];
                        }
                    }
                    if(binder.queryParametersPlaces.size()>0)
                    {
                        auto qureyPara=req.getParameters();
                        for(auto parameter:qureyPara)
                        {
                            if(binder.queryParametersPlaces.find(parameter.first)!=
                                    binder.queryParametersPlaces.end())
                            {
                                auto place=binder.queryParametersPlaces[parameter.first];
                                if(place>params.size())
                                    params.resize(place);
                                params[place-1]=parameter.second;
                            }
                        }
                    }
                    std::list<std::string> paraList;
                    for(auto p:params)
                    {
                        LOG_DEBUG<<p;
                        paraList.push_back(std::move(p));
                    }

                    binder.binderPtr->handleHttpApiRequest(paraList,req,callback);
                    return;
                }
            }
        }
    }
    auto res=drogon::HttpResponse::notFoundResponse();

    if(needSetJsessionid)
            res->addCookie("JSESSIONID",session_id);

    callback(*res);

}

void HttpAppFrameworkImpl::readSendFile(const std::string& filePath,const HttpRequest& req, HttpResponse* resp)
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

            std::string modiStr=req.getHeader("If-Modified-Since");
            if(modiStr!=""&&modiStr==lastModified)
            {
                LOG_DEBUG<<"not Modified!";
                resp->setStatusCode(HttpResponse::k304NotModified);
                return;
            }
            resp->addHeader("Last-Modified",lastModified);

            resp->addHeader("Expires","Thu, 01 Jan 1970 00:00:00 GMT");
        }
    }

    std::streambuf * pbuf = infile.rdbuf();
    std::streamsize size = pbuf->pubseekoff(0,infile.end);
    pbuf->pubseekoff(0,infile.beg);       // rewind
    std::string str;
    str.resize(size);
    pbuf->sgetn (&str[0],size);
    infile.close();

    resp->setStatusCode(HttpResponse::k200Ok);
    LOG_INFO << "file len:" << str.length();
    resp->setBody(std::move(str));

}

std::string HttpAppFrameworkImpl::getUuid()
{
    uuid_t uu;
    uuid_generate(uu);
    return stringToHex(uu, 16);
}

std::string HttpAppFrameworkImpl::stringToHex(unsigned char* ptr, long long length)
{
    std::string idString;
    for (long long i = 0; i < length; i++)
    {
        int value = (ptr[i] & 0xf0)>>4;
        if (value < 10)
        {
            idString.append(1, char(value + 48));
        } else
        {
            idString.append(1, char(value + 55));
        }

        value = (ptr[i] & 0x0f);
        if (value < 10)
        {
            idString.append(1, char(value + 48));
        } else
        {
            idString.append(1, char(value + 55));
        }
    }
    return idString;
}



HttpAppFramework& HttpAppFramework::instance() {
    static HttpAppFrameworkImpl _instance;
    return _instance;
}

HttpAppFramework::~HttpAppFramework()
{

}




