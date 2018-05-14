// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.
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

namespace drogon
{

    class HttpAppFrameworkImpl:public HttpAppFramework
    {
    public:
        virtual void setListening(const std::string &ip,uint16_t port) override;
        virtual void run() override ;
        virtual void registerHttpSimpleController(const std::string &pathName,const std::string &crtlName,const std::vector<std::string> &filters=
        std::vector<std::string>())override ;
        ~HttpAppFrameworkImpl(){}
    private:
        std::string _ip;
        uint16_t _port;
        void onAsyncRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback);
        void readSendFile(const std::string& filePath,const HttpRequest& req, HttpResponse* resp);

        //if uuid package found,we can use a uuid string as session id;
        //set _sessionTimeout=0 to disable location session control based on cookies;
        uint _sessionTimeout=0;
        typedef std::shared_ptr<Session> SessionPtr;
        std::unique_ptr<CacheMap<std::string,SessionPtr>> _sessionMapPtr;

        //
        struct ControllerAndFiltersName
        {
            std::string controllerName;
            std::vector<std::string> filtersName;
        };
        std::unordered_map<std::string,ControllerAndFiltersName>_simpCtrlMap;
        std::mutex _simpCtrlMutex;

        bool _enableLastModify=true;
        std::set<std::string> _fileTypeSet={"html","jpg"};
        std::string _rootPath;



        //tool funcs

        std::string getUuid();
        std::string stringToHex(unsigned char* ptr, long long length);

    };
}

using namespace drogon;
using namespace std::placeholders;
HttpAppFrameworkImpl * HttpAppFramework::_implPtr= nullptr;
std::once_flag HttpAppFramework::_once;

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
void HttpAppFrameworkImpl::setListening(const std::string &ip,uint16_t port)
{
    _ip=ip;
    _port=port;
}

void HttpAppFrameworkImpl::run()
{
    trantor::EventLoop loop;
    HttpServer httpServer(&loop,InetAddress(_ip,_port),"");
    httpServer.setHttpAsyncCallback(std::bind(&HttpAppFrameworkImpl::onAsyncRequest,this,_1,_2));
    _sessionMapPtr=std::unique_ptr<CacheMap<std::string,SessionPtr>>(new CacheMap<std::string,SessionPtr>(&loop,1,1200));
    httpServer.start();
    loop.loop();
}


void HttpAppFrameworkImpl::onAsyncRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)
{
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
    if(_sessionTimeout>0)
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
    }

    std::string path = req.path();
    auto pos = path.rfind(".");
    if(pos != std::string::npos) {
        std::string filetype = path.substr(pos + 1, path.length());
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
    ((HttpRequestImpl &)req).setSession((*_sessionMapPtr)[session_id]);
    /*filters
    std::vector<std::string> filters=(*_filterMap)[req.path().c_str()];
    for(std::string filterClassName:filters)
    {
        TRObject *_obj=TRClassMap::newObject(filterClassName);
        HttpFilter *filter= dynamic_cast<HttpFilter*>(_obj);
        int filter_flag=0;
        if(filter)
        {
            LOG_INFO<<filterClassName<<".doFilter()";
            filter->setSession(_sessionMap[session_id]);
            filter->doFilter(req,[=,&filter_flag]( HttpResponse &resp){
                callback(resp);
                filter_flag=1;
            });
            if(filter_flag)
                return;
        }
        else
        LOG_INFO<<"can't find filter "<<filterClassName;
    }
     */
    /*find controller*/
    std::string pathLower(req.path());
    std::transform(pathLower.begin(),pathLower.end(),pathLower.begin(),tolower);
    if(_simpCtrlMap.find(pathLower)!=_simpCtrlMap.end())
    {
        auto filters=_simpCtrlMap[pathLower].filtersName;
        LOG_TRACE<<"filters count:"<<filters.size();
        for(auto filter:filters)
        {
            LOG_TRACE<<"filters in path("<<pathLower<<"):"<<filter;
            auto _object=std::shared_ptr<DrObjectBase>(DrClassMap::newObject(filter));
            auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
            if(_filter)
            {
                auto resPtr=_filter->doFilter(req);
                if(resPtr)
                {
                    callback(*resPtr);
                    return;
                }
            } else
            {
                LOG_ERROR<<"filter "<<filter<<" not found";
            }
        }
        std::string ctrlName = _simpCtrlMap[pathLower].controllerName;

        auto _object = std::shared_ptr<DrObjectBase>(DrClassMap::newObject(ctrlName));

        auto controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);

        if(controller) {
//            controller->setSession((*_sessionMapPtr)[session_id]);
//            controller->setEnvironment(this);
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

    HttpResponseImpl resp;

    resp.setStatusCode(HttpResponse::k404NotFound);
    //resp.setCloseConnection(true);

    if(needSetJsessionid)
            resp.addCookie("JSESSIONID",session_id);

    callback(resp);
    // }

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
    char *contents = new char [size];
    pbuf->sgetn (contents,size);
    infile.close();
    std::string str(contents,size);




    resp->setStatusCode(HttpResponse::k200Ok);
    LOG_INFO << "file len:" << str.length();
    resp->setBody(str);
    delete contents;
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
    std::call_once(_once,[]{
        _implPtr=new HttpAppFrameworkImpl;
    });
    return *_implPtr;
}

HttpAppFramework::~HttpAppFramework()
{

}



