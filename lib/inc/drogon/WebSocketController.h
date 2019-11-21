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

#define WS_PATH_LIST_BEGIN                                               \
    static std::vector<std::pair<std::string, std::vector<std::string>>> \
    paths()                                                              \
    {                                                                    \
        std::vector<std::pair<std::string, std::vector<std::string>>> vet;

#define WS_PATH_ADD(path, filters...) vet.push_back({path, {filters}})

#define WS_PATH_LIST_END \
    return vet;          \
    }

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
    static void initPathRouting()
    {
        auto vPaths = T::paths();
        for (auto const &path : vPaths)
        {
            LOG_TRACE << "register websocket controller ("
                      << WebSocketController<T>::classTypeName()
                      << ") on path:" << path.first;
            app().registerWebSocketController(
                path.first,
                WebSocketController<T>::classTypeName(),
                path.second);
        }
    }

  protected:
    WebSocketController()
    {
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

}  // namespace drogon
