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
#include <drogon/utils/HttpConstraint.h>
#include <drogon/HttpAppFramework.h>
#include <iostream>
#include <string>
#include <trantor/utils/Logger.h>
#include <vector>

/// For more details on the class, see the wiki site (the 'HttpController'
/// section)

#define METHOD_LIST_BEGIN         \
    static void initPathRouting() \
    {
#define METHOD_ADD(method, pattern, ...) \
    registerMethod(&method, pattern, {__VA_ARGS__}, true, #method)
#define ADD_METHOD_TO(method, path_pattern, ...) \
    registerMethod(&method, path_pattern, {__VA_ARGS__}, false, #method)
#define ADD_METHOD_VIA_REGEX(method, regex, ...) \
    registerMethodViaRegex(&method, regex, {__VA_ARGS__}, #method)
#define METHOD_LIST_END \
    return;             \
    }

namespace drogon
{
/**
 * @brief The base class for HTTP controllers.
 *
 */
class HttpControllerBase
{
};

/**
 * @brief The reflection base class template for HTTP controllers
 *
 * @tparam T the type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have nondefault constructors.
 */
template <typename T, bool AutoCreation = true>
class HttpController : public DrObject<T>, public HttpControllerBase
{
  public:
    static constexpr bool isAutoCreation = AutoCreation;

  protected:
    template <typename FUNCTION>
    static void registerMethod(
        FUNCTION &&function,
        const std::string &pattern,
        const std::vector<internal::HttpConstraint> &constraints = {},
        bool classNameInPath = true,
        const std::string &handlerName = "")
    {
        if (classNameInPath)
        {
            std::string path = "/";
            path.append(HttpController<T, AutoCreation>::classTypeName());
            LOG_TRACE << "classname:"
                      << HttpController<T, AutoCreation>::classTypeName();

            // transform(path.begin(), path.end(), path.begin(), [](unsigned
            // char c){ return tolower(c); });
            std::string::size_type pos;
            while ((pos = path.find("::")) != std::string::npos)
            {
                path.replace(pos, 2, "/");
            }
            if (pattern.empty() || pattern[0] == '/')
                app().registerHandler(path + pattern,
                                      std::forward<FUNCTION>(function),
                                      constraints,
                                      handlerName);
            else
                app().registerHandler(path + "/" + pattern,
                                      std::forward<FUNCTION>(function),
                                      constraints,
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
                                  constraints,
                                  handlerName);
        }
    }

    template <typename FUNCTION>
    static void registerMethodViaRegex(
        FUNCTION &&function,
        const std::string &regExp,
        const std::vector<internal::HttpConstraint> &constraints =
            std::vector<internal::HttpConstraint>{},
        const std::string &handlerName = "")
    {
        app().registerHandlerViaRegex(regExp,
                                      std::forward<FUNCTION>(function),
                                      constraints,
                                      handlerName);
    }

  private:
    class methodRegistrator
    {
      public:
        methodRegistrator()
        {
            if (AutoCreation)
                T::initPathRouting();
        }
    };

    // use static value to register controller method in framework before
    // main();
    static methodRegistrator registrator_;

    virtual void *touch()
    {
        return &registrator_;
    }
};

template <typename T, bool AutoCreation>
typename HttpController<T, AutoCreation>::methodRegistrator
    HttpController<T, AutoCreation>::registrator_;
}  // namespace drogon
