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
#define PATH_LIST_BEGIN       \
    static void ___paths___() \
    {

#define PATH_ADD(path, filters...) __registerSelf(path, {filters});

#define PATH_LIST_END \
    }
namespace drogon
{
class HttpSimpleControllerBase : public virtual DrObjectBase
{
  public:
    virtual void asyncHandleHttpRequest(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback) = 0;
    virtual ~HttpSimpleControllerBase() {}
};

template <typename T>
class HttpSimpleController : public DrObject<T>, public HttpSimpleControllerBase
{
  public:
    virtual ~HttpSimpleController() {}

  protected:
    HttpSimpleController() {}
    static void __registerSelf(const std::string &path, const std::vector<any> &filtersAndMethods)
    {
        LOG_TRACE << "register simple controller(" << HttpSimpleController<T>::classTypeName() << ") on path:" << path;
        HttpAppFramework::instance().registerHttpSimpleController(path, HttpSimpleController<T>::classTypeName(), filtersAndMethods);
    }

  private:
    class pathRegister
    {
      public:
        pathRegister()
        {
            T::___paths___();
        }

      protected:
        void _register(const std::string &className, const std::vector<std::string> &paths)
        {
            for (auto reqPath : paths)
            {
                std::cout << "register controller class " << className << " on path " << reqPath << std::endl;
            }
        }
    };
    friend pathRegister;
    static pathRegister _register;
    virtual void *touch()
    {
        return &_register;
    }
};
template <typename T>
typename HttpSimpleController<T>::pathRegister HttpSimpleController<T>::_register;

} // namespace drogon
