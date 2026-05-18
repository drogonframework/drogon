/**
 *
 *  WebSocketController.h
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
#include <drogon/WebSocketConnection.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

#define WS_PATH_LIST_BEGIN        \
    static void initPathRouting() \
    {
#define WS_PATH_ADD(path, ...) registerSelf__(path, {__VA_ARGS__})
#define WS_ADD_PATH_VIA_REGEX(regExp, ...) \
    registerSelfRegex__(regExp, {__VA_ARGS__})
#define WS_PATH_LIST_END }
#define WS_CORO_PATH_LIST_BEGIN WS_PATH_LIST_BEGIN
#define WS_CORO_PATH_ADD(path, ...) WS_PATH_ADD(path, __VA_ARGS__)
#define WS_CORO_ADD_PATH_VIA_REGEX(regExp, ...) \
    WS_ADD_PATH_VIA_REGEX(regExp, __VA_ARGS__)
#define WS_CORO_PATH_LIST_END WS_PATH_LIST_END

namespace drogon
{
/**
 * @brief The abstract base class for WebSocket controllers.
 *
 */
class WebSocketControllerBase : public virtual DrObjectBase
{
  public:
    // This function is called when a new message is received
    virtual void handleNewMessage(const WebSocketConnectionPtr &,
                                  std::string &&,
                                  const WebSocketMessageType &) = 0;

    // This function is called after a new connection of WebSocket is
    // established.
    virtual void handleNewConnection(const HttpRequestPtr &,
                                     const WebSocketConnectionPtr &) = 0;

    // This function is called after a WebSocket connection is closed
    virtual void handleConnectionClosed(const WebSocketConnectionPtr &) = 0;

    virtual ~WebSocketControllerBase()
    {
    }
};

using WebSocketControllerBasePtr = std::shared_ptr<WebSocketControllerBase>;

/**
 * @brief The reflection base class template for WebSocket controllers
 *
 * @tparam T the type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have nondefault constructors.
 */
template <typename T, bool AutoCreation = true>
class WebSocketController : public DrObject<T>, public WebSocketControllerBase
{
  public:
    static const bool isAutoCreation = AutoCreation;

    virtual ~WebSocketController()
    {
    }

  protected:
    WebSocketController()
    {
    }

    static void registerSelf__(
        const std::string &path,
        const std::vector<internal::HttpConstraint> &constraints)
    {
        LOG_TRACE << "register websocket controller("
                  << WebSocketController<T, AutoCreation>::classTypeName()
                  << ") on path:" << path;
        app().registerWebSocketController(
            path,
            WebSocketController<T, AutoCreation>::classTypeName(),
            constraints);
    }

    static void registerSelfRegex__(
        const std::string &regExp,
        const std::vector<internal::HttpConstraint> &constraints)
    {
        LOG_TRACE << "register websocket controller("
                  << WebSocketController<T, AutoCreation>::classTypeName()
                  << ") on regExp:" << regExp;
        app().registerWebSocketControllerRegex(
            regExp,
            WebSocketController<T, AutoCreation>::classTypeName(),
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
typename WebSocketController<T, AutoCreation>::pathRegistrator
    WebSocketController<T, AutoCreation>::registrator_;

#ifdef __cpp_impl_coroutine
/**
 * @brief The abstract base class for coroutine WebSocket controllers.
 */
class WebSocketCoroControllerBase : public WebSocketControllerBase
{
  public:
    ~WebSocketCoroControllerBase() override = default;

    virtual Task<> handleNewMessageCoro(const WebSocketConnectionPtr &,
                                        std::string &&,
                                        const WebSocketMessageType &) = 0;

    virtual Task<> handleNewConnectionCoro(const HttpRequestPtr &,
                                           const WebSocketConnectionPtr &) = 0;

    virtual Task<> handleConnectionClosedCoro(
        const WebSocketConnectionPtr &) = 0;
};

/**
 * @brief Reflection base class template for coroutine WebSocket controllers.
 */
template <typename T, bool AutoCreation = true>
class WebSocketCoroController : public DrObject<T>,
                                public WebSocketCoroControllerBase
{
  public:
    static const bool isAutoCreation = AutoCreation;

    virtual ~WebSocketCoroController()
    {
    }

    void handleNewMessage(const WebSocketConnectionPtr &conn,
                          std::string &&message,
                          const WebSocketMessageType &type) final
    {
        auto objPtr = DrClassMap::getSingleInstance<T>();
        drogon::async_run([objPtr,
                           conn,
                           message = std::move(message),
                           type]() mutable -> Task<> {
            co_await objPtr->handleNewMessageCoro(conn,
                                                  std::move(message),
                                                  type);
        });
    }

    void handleNewConnection(const HttpRequestPtr &req,
                             const WebSocketConnectionPtr &conn) final
    {
        auto objPtr = DrClassMap::getSingleInstance<T>();
        drogon::async_run([objPtr, req, conn]() mutable -> Task<> {
            co_await objPtr->handleNewConnectionCoro(req, conn);
        });
    }

    void handleConnectionClosed(const WebSocketConnectionPtr &conn) final
    {
        auto objPtr = DrClassMap::getSingleInstance<T>();
        drogon::async_run([objPtr, conn]() mutable -> Task<> {
            co_await objPtr->handleConnectionClosedCoro(conn);
        });
    }

  protected:
    WebSocketCoroController()
    {
    }

    static void registerSelf__(
        const std::string &path,
        const std::vector<internal::HttpConstraint> &constraints)
    {
        LOG_TRACE << "register websocket coro controller("
                  << WebSocketCoroController<T, AutoCreation>::classTypeName()
                  << ") on path:" << path;
        app().registerWebSocketController(
            path,
            WebSocketCoroController<T, AutoCreation>::classTypeName(),
            constraints);
    }

    static void registerSelfRegex__(
        const std::string &regExp,
        const std::vector<internal::HttpConstraint> &constraints)
    {
        LOG_TRACE << "register websocket coro controller("
                  << WebSocketCoroController<T, AutoCreation>::classTypeName()
                  << ") on regExp:" << regExp;
        app().registerWebSocketControllerRegex(
            regExp,
            WebSocketCoroController<T, AutoCreation>::classTypeName(),
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
typename WebSocketCoroController<T, AutoCreation>::pathRegistrator
    WebSocketCoroController<T, AutoCreation>::registrator_;
#endif

}  // namespace drogon
