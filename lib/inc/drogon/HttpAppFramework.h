//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

//#include <drogon/HttpRequest.h>
//#include <drogon/HttpResponse.h>
//#include <drogon/CacheMap.h>
//#include <drogon/Session.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/PostFilter.h>
#include <memory>
#include <string>
#include <functional>

namespace drogon
{

    class HttpSimpleControllerBase:public virtual DrObjectBase
    {
    public:
        virtual void asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)=0;
        virtual ~HttpSimpleControllerBase(){}
    };
    class HttpAppFramework:public trantor::NonCopyable
    {
    public:
        static HttpAppFramework &instance();
        virtual void setListening(const std::string &ip,uint16_t port)=0;
        virtual void run()=0;
        virtual ~HttpAppFramework();
        virtual void registerHttpSimpleController(const std::string &pathName,const std::string &crtlName,const std::vector<std::string> &filters=
                std::vector<std::string>())=0;

    private:
        static std::once_flag _once;
        static class HttpAppFrameworkImpl *_implPtr;
    };
}
