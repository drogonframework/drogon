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
#include <drogon/utils/HttpConstraint.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <string>
#include <vector>
#define PATH_LIST_BEGIN           \
    static void initPathRouting() \
    {
#define PATH_ADD(path, filters...) __registerSelf(path, {filters});

#define PATH_LIST_END }
namespace drogon
{
/**
 * @brief The abstract base class for HTTP simple controllers.
 *
 */
class HttpSimpleControllerBase : public virtual DrObjectBase
{
  public:
    /**
     * @brief The function is called when a HTTP request is routed to the
     * controller.
     *
     * @param req The HTTP request.
     * @param callback The callback via which a response is returned.
     */
    virtual void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) = 0;
    virtual ~HttpSimpleControllerBase()
    {
    }
};

/**
 * @brief The reflection base class template for HTTP simple controllers
 *
 * @tparam T The type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have nondefault constructors.
 */
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
    static void __registerSelf(
        const std::string &path,
        const std::vector<internal::HttpConstraint> &filtersAndMethods)
    {
        LOG_TRACE << "register simple controller("
                  << HttpSimpleController<T>::classTypeName()
                  << ") on path:" << path;
        app().registerHttpSimpleController(
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
typename HttpSimpleController<T, AutoCreation>::pathRegister
    HttpSimpleController<T, AutoCreation>::_register;

}  // namespace drogon
