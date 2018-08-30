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

#define METHOD_LIST_BEGIN \
static void initMethods() \
{
  #define METHOD_ADD(method,pattern,filters...) \
  {\
    std::string methodName="";\
    registerMethod(methodName,pattern,&method,{filters});\
  }

  #define METHOD_LIST_END \
  return;\
}\
protected:

namespace drogon
{
    template <typename T>
    class HttpApiController:public DrObject<T>
    {
    protected:
        template <typename FUNCTION>
        static void registerMethod(const std::string &methodName,const std::string &pattern,FUNCTION &&function,const std::vector<std::string> &filters)
        {
            std::string path=std::string("/")+HttpApiController<T>::classTypeName();
            LOG_DEBUG<<"classname:"<<HttpApiController<T>::classTypeName();
            if(!methodName.empty())
            {
                path.append("/");
                path.append(methodName);
            }
            transform(path.begin(), path.end(), path.begin(), tolower);
            std::string::size_type pos;
            while((pos=path.find("::"))!=std::string::npos)
            {
                path.replace(pos,2,"/");
            }
            if(pattern[0]=='/')
                HttpAppFramework::registerHttpApiMethod(path+pattern,
                                                        std::forward<FUNCTION>(function),
                                                                filters);
            else
                HttpAppFramework::registerHttpApiMethod(path+"/"+pattern,
                                                        std::forward<FUNCTION>(function),
                                                        filters);
        }
    private:

        class methodRegister {
        public:
            methodRegister()
            {
                T::initMethods();
            }
        };
        //use static value to register controller method in framework before main();
        static methodRegister _register;
        virtual void* touch()
        {
            return &_register;
        }
    };
    template <typename T> typename HttpApiController<T>::methodRegister HttpApiController<T>::_register;
}
