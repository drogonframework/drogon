/**
 *
 *  HttpController.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <string>
#include <vector>
#include <iostream>

/// For more details on the class, see the wiki site (the 'HttpController' section)

#define METHOD_LIST_BEGIN         \
    static void initPathRouting() \
    {

#define METHOD_ADD(method, pattern, filters...) \
    registerMethod(&method, pattern, {filters}, true, #method)

#define ADD_METHOD_TO(method, path_pattern, filters...) \
    registerMethod(&method, path_pattern, {filters}, false, #method)

#define METHOD_LIST_END \
    return;             \
    }

namespace drogon
{

class HttpControllerBase
{
};

template <typename T, bool AutoCreation = true>
class HttpController : public DrObject<T>, public HttpControllerBase
{
  public:
    static const bool isAutoCreation = AutoCreation;

  protected:
      template <typename FUNCTION>
      static void registerMethod(FUNCTION &&function,
                                 const std::string &pattern,
                                 const std::vector<any> &filtersAndMethods = std::vector<any>(),
                                 bool classNameInPath = true,
                                 const std::string &handlerName = "")
      {
          if (classNameInPath)
          {
              std::string path = "/";
              path.append(HttpController<T>::classTypeName());
              LOG_TRACE << "classname:" << HttpController<T>::classTypeName();

              //transform(path.begin(), path.end(), path.begin(), tolower);
              std::string::size_type pos;
              while ((pos = path.find("::")) != std::string::npos)
              {
                  path.replace(pos, 2, "/");
              }
              if (pattern.empty() || pattern[0] == '/')
                  app().registerHandler(path + pattern,
                                        std::forward<FUNCTION>(function),
                                        filtersAndMethods,
                                        handlerName);
              else
                  app().registerHandler(path + "/" + pattern,
                                        std::forward<FUNCTION>(function),
                                        filtersAndMethods,
                                        handlerName);
          }
          else
          {
              std::string path = pattern;
              if (path.empty() || path[0] != '/')
              {
                  path = "/" + path;
              }
              app().registerHandler(path,
                                    std::forward<FUNCTION>(function),
                                    filtersAndMethods,
                                    handlerName);
          }
    }

  private:
    class methodRegister
    {
      public:
        methodRegister()
        {
            if (AutoCreation)
                T::initPathRouting();
        }
    };
    //use static value to register controller method in framework before main();
    static methodRegister _register;
    virtual void *touch()
    {
        return &_register;
    }
};
template <typename T, bool AutoCreation>
typename HttpController<T, AutoCreation>::methodRegister HttpController<T, AutoCreation>::_register;
} // namespace drogon
