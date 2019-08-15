#pragma once

#include <memory>
#include <functional>

namespace drogon
{
class HttpRequest;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
class HttpResponse;
typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
class Cookie;
class Session;
typedef std::shared_ptr<Session> SessionPtr;
class UploadFile;
class WebSocketControllerBase;
typedef std::shared_ptr<WebSocketControllerBase> WebSocketControllerBasePtr;
class HttpFilterBase;
typedef std::shared_ptr<HttpFilterBase> HttpFilterBasePtr;
class HttpSimpleControllerBase;
typedef std::shared_ptr<HttpSimpleControllerBase> HttpSimpleControllerBasePtr;
class HttpRequestImpl;
typedef std::shared_ptr<HttpRequestImpl> HttpRequestImplPtr;
class HttpResponseImpl;
typedef std::shared_ptr<HttpResponseImpl> HttpResponseImplPtr;
class WebSocketConnectionImpl;
typedef std::shared_ptr<WebSocketConnectionImpl> WebSocketConnectionImplPtr;
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
typedef std::shared_ptr<DbClient> DbClientPtr;
class DbClientManager;
}  // namespace orm
}  // namespace drogon

namespace trantor
{
class EventLoop;
class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
class Resolver;
}  // namespace trantor

namespace drogon
{
typedef std::function<void(const HttpRequestImplPtr &,
                           std::function<void(const HttpResponsePtr &)> &&)>
    HttpAsyncCallback;
typedef std::function<void(const HttpRequestImplPtr &,
                           std::function<void(const HttpResponsePtr &)> &&,
                           const WebSocketConnectionImplPtr &)>
    WebSocketNewAsyncCallback;
}  // namespace drogon