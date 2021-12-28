#pragma once

#include <memory>
#include <functional>

namespace drogon
{
class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;
class HttpResponse;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;
class Cookie;
class Session;
using SessionPtr = std::shared_ptr<Session>;
class UploadFile;
class WebSocketControllerBase;
using WebSocketControllerBasePtr = std::shared_ptr<WebSocketControllerBase>;
class HttpFilterBase;
using HttpFilterBasePtr = std::shared_ptr<HttpFilterBase>;
class HttpSimpleControllerBase;
using HttpSimpleControllerBasePtr = std::shared_ptr<HttpSimpleControllerBase>;
class HttpRequestImpl;
using HttpRequestImplPtr = std::shared_ptr<HttpRequestImpl>;
class HttpResponseImpl;
using HttpResponseImplPtr = std::shared_ptr<HttpResponseImpl>;
class WebSocketConnectionImpl;
using WebSocketConnectionImplPtr = std::shared_ptr<WebSocketConnectionImpl>;
class HttpRequestParser;
class StaticFileRouter;
class HttpControllersRouter;
class WebsocketControllersRouter;
class HttpSimpleControllersRouter;
class PluginsManager;
class ListenerManager;
class SharedLibManager;
class SessionManager;
class HttpServer;

namespace orm
{
class DbClient;
using DbClientPtr = std::shared_ptr<DbClient>;
class DbClientManager;
}  // namespace orm
namespace nosql
{
class RedisClient;
using RedisClientPtr = std::shared_ptr<RedisClient>;
class RedisClientManager;
}  // namespace nosql
}  // namespace drogon

namespace trantor
{
class EventLoop;
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
class Resolver;
class AsyncFileLogger;
}  // namespace trantor

namespace drogon
{
using HttpAsyncCallback =
    std::function<void(const HttpRequestImplPtr &,
                       std::function<void(const HttpResponsePtr &)> &&)>;
using WebSocketNewAsyncCallback =
    std::function<void(const HttpRequestImplPtr &,
                       std::function<void(const HttpResponsePtr &)> &&,
                       const WebSocketConnectionImplPtr &)>;
}  // namespace drogon
