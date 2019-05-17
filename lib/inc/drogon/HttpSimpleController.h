/**
 *
 *  HttpSimpleController.h
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
#include <iostream>
#include <string>
#include <trantor/utils/Logger.h>
#include <vector>
#define PATH_LIST_BEGIN           \
    static void initPathRouting() \
    {
#define PATH_ADD(path, filters...) __registerSelf(path, {filters});

#define PATH_LIST_END }
namespace drogon
{
class HttpSimpleControllerBase : public virtual DrObjectBase
{
  public:
    virtual void asyncHandleHttpRequest(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) = 0;
    virtual ~HttpSimpleControllerBase()
    {
    }
};

template <typename T, bool AutoCreation = true>
class HttpSimpleController : public DrObject<T>, public HttpSimpleControllerBase
{
  public:
    static const bool isAutoCreation = AutoCreation;
    virtual ~HttpSimpleController()
    {
    }

  protected:
    HttpSimpleController()
    {
    }
    static void __registerSelf(const std::string &path, const std::vector<any> &filtersAndMethods)
    {
        LOG_TRACE << "register simple controller(" << HttpSimpleController<T>::classTypeName() << ") on path:" << path;
        HttpAppFramework::instance().registerHttpSimpleController(
            path, HttpSimpleController<T>::classTypeName(), filtersAndMethods);
    }

  private:
    class pathRegister
    {
      public:
        pathRegister()
        {
            if (AutoCreation)
            {
                T::initPathRouting();
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
template <typename T, bool AutoCreation>
typename HttpSimpleController<T, AutoCreation>::pathRegister HttpSimpleController<T, AutoCreation>::_register;

}  // namespace drogon
