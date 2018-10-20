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

#define METHOD_LIST_BEGIN     \
    static void initMethods() \
    {

#define METHOD_ADD(method, pattern, filters...)      \
    {                                                \
        registerMethod(&method, pattern, {filters}); \
    }

#define METHOD_LIST_END \
    return;             \
    }                   \
                        \
  protected:

namespace drogon
{
template <typename T>
class HttpApiController : public DrObject<T>
{
  protected:
    template <typename FUNCTION>
    static void registerMethod(FUNCTION &&function,
                               const std::string &pattern,
                               const std::vector<any> &filtersAndMethods = std::vector<any>())
    {
        std::string path = std::string("/") + HttpApiController<T>::classTypeName();
        LOG_TRACE << "classname:" << HttpApiController<T>::classTypeName();

        //transform(path.begin(), path.end(), path.begin(), tolower);
        std::string::size_type pos;

        std::vector<HttpRequest::Method> validMethods;
        std::vector<std::string> filters;
        for (auto &filterOrMethod : filtersAndMethods)
        {
            if (filterOrMethod.type() == typeid(std::string))
            {
                filters.push_back(*any_cast<std::string>(&filterOrMethod));
            }
            else if (filterOrMethod.type() == typeid(const char *))
            {
                filters.push_back(*any_cast<const char *>(&filterOrMethod));
            }
            else if (filterOrMethod.type() == typeid(HttpRequest::Method))
            {
                validMethods.push_back(*any_cast<HttpRequest::Method>(&filterOrMethod));
            }
            else
            {
                std::cerr << "Invalid controller constraint type:" << filterOrMethod.type().name() << std::endl;
                LOG_ERROR << "Invalid controller constraint type";
                exit(1);
            }
        }
        while ((pos = path.find("::")) != std::string::npos)
        {
            path.replace(pos, 2, "/");
        }
        if (pattern[0] == '/')
            HttpAppFramework::registerHttpApiMethod(path + pattern,
                                                    std::forward<FUNCTION>(function),
                                                    validMethods,
                                                    filters);
        else
            HttpAppFramework::registerHttpApiMethod(path + "/" + pattern,
                                                    std::forward<FUNCTION>(function),
                                                    validMethods,
                                                    filters);
    }

  private:
    class methodRegister
    {
      public:
        methodRegister()
        {
            T::initMethods();
        }
    };
    //use static value to register controller method in framework before main();
    static methodRegister _register;
    virtual void *touch()
    {
        return &_register;
    }
};
template <typename T>
typename HttpApiController<T>::methodRegister HttpApiController<T>::_register;
} // namespace drogon
