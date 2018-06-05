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

#include <drogon/DrObject.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <string>
#include <vector>
#include <iostream>
#define PATH_LIST_BEGIN public:\
static std::vector<std::pair<std::string,std::vector<std::string>>> paths() \
{\
    std::vector<std::pair<std::string,std::vector<std::string>>> vet;

#define PATH_ADD(path,filters...) \
    vet.push_back({path,{filters}})

#define PATH_LIST_END \
return vet;\
}
namespace drogon
{
    class HttpSimpleControllerBase:public virtual DrObjectBase
    {
    public:
        virtual void asyncHandleHttpRequest(const HttpRequest& req,const std::function<void (HttpResponse &)> &callback)=0;
        virtual ~HttpSimpleControllerBase(){}
    };


    template <typename T>
    class HttpSimpleController:public DrObject<T>,public HttpSimpleControllerBase
    {
    public:

        virtual ~HttpSimpleController(){}

    protected:

        HttpSimpleController(){}

    private:

        class pathRegister {
        public:
        pathRegister()
        {
            auto vPaths=T::paths();

            for(auto path:vPaths)
            {
                LOG_DEBUG<<"register simple controller("<<HttpSimpleController<T>::classTypeName()<<") on path:"<<path.first;
                HttpAppFramework::instance().registerHttpSimpleController(path.first,HttpSimpleController<T>::classTypeName(),path.second);
            }

        }

        protected:
            void _register(const std::string& className,const std::vector<std::string> &paths)
            {
                for(auto reqPath:paths)
                {
                    std::cout<<"register controller class "<<className<<" on path "<<reqPath<<std::endl;
                }
            }
        };
        friend pathRegister;
        static pathRegister _register;
        virtual void* touch()
        {
            return &_register;
        }
    };
    template <typename T> typename HttpSimpleController<T>::pathRegister HttpSimpleController<T>::_register;

}
