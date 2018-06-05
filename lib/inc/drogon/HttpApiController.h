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

#define METHOD_LIST_BEGIN public:\
static void initMethods() \
{
  #define METHOD_ADD(method,pattern,isMethodNameInPath,filters...) \
  {\
    std::string methodName="";\
    if(isMethodNameInPath)\
    {\
      methodName=std::string(#method);\
      auto pos=methodName.find("::");\
      if(pos!=std::string::npos)\
      {\
        methodName=methodName.substr(pos+2);\
      }\
    }\
    registerMethod(methodName,pattern,&method,{filters});\
  }

  #define METHOD_LIST_END \
  return;\
}
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
            path.append("/");
            path.append(methodName);
            transform(path.begin(), path.end(), path.begin(), tolower);
            std::string::size_type pos;
            while((pos=path.find("::"))!=std::string::npos)
            {
                path.replace(pos,2,"/");
            }
            HttpAppFramework::registerHttpApiMethod(path,pattern,std::forward<FUNCTION>(function),filters);
        }
    private:

        class methodRegister {
        public:
            methodRegister()
            {
                T::initMethods();
            }
        };
        static methodRegister _register;
        virtual void* touch()
        {
            return &_register;
        }
    };
    template <typename T> typename HttpApiController<T>::methodRegister HttpApiController<T>::_register;
}
