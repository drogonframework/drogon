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
#define PATH_ADD(path, ...) registerSelf__(path, {__VA_ARGS__})
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

    static void registerSelf__(
        const std::string &path,
        const std::vector<internal::HttpConstraint> &constraints)
    {
        LOG_TRACE << "register simple controller("
                  << HttpSimpleController<T, AutoCreation>::classTypeName()
                  << ") on path:" << path;
        app().registerHttpSimpleController(
            path,
            HttpSimpleController<T, AutoCreation>::classTypeName(),
            constraints);
    }

  private:
    class pathRegistrator
    {
      public:
        pathRegistrator()
        {
            if (AutoCreation)
            {
                T::initPathRouting();
            }
        }
    };

    friend pathRegistrator;
    static pathRegistrator registrator_;

    virtual void *touch()
    {
        return &registrator_;
    }
};

template <typename T, bool AutoCreation>
typename HttpSimpleController<T, AutoCreation>::pathRegistrator
    HttpSimpleController<T, AutoCreation>::registrator_;

}  // namespace drogon
