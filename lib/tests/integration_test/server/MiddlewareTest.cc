#include <drogon/HttpController.h>
#include <drogon/HttpMiddleware.h>
using namespace drogon;

class Middleware1 : public drogon::HttpMiddleware<Middleware1>
{
  public:
    Middleware1()
    {
        // do not omit constructor
        void(0);
    };

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override
    {
        auto ptr = std::make_shared<std::string>("1");
        req->attributes()->insert("test-middleware", ptr);

        nextCb([req, ptr, mcb = std::move(mcb)](const HttpResponsePtr &resp) {
            ptr->append("1");
            resp->setBody(*ptr);
            mcb(resp);
        });
    }
};

class Middleware2 : public drogon::HttpMiddleware<Middleware2>
{
  public:
    Middleware2()
    {
        // do not omit constructor
        void(0);
    };

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override
    {
        auto ptr = req->attributes()->get<std::shared_ptr<std::string>>(
            "test-middleware");
        ptr->append("2");

        nextCb([req, ptr, mcb = std::move(mcb)](const HttpResponsePtr &resp) {
            ptr->append("2");
            resp->setBody(*ptr);
            mcb(resp);
        });
    }
};

class Middleware3 : public drogon::HttpMiddleware<Middleware3>
{
  public:
    Middleware3()
    {
        // do not omit constructor
        void(0);
    };

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override
    {
        auto ptr = req->attributes()->get<std::shared_ptr<std::string>>(
            "test-middleware");
        ptr->append("3");

        nextCb([req, ptr, mcb = std::move(mcb)](const HttpResponsePtr &resp) {
            ptr->append("3");
            resp->setBody(*ptr);
            mcb(resp);
        });
    }
};

class MiddlewareBlock : public drogon::HttpMiddleware<MiddlewareBlock>
{
  public:
    MiddlewareBlock()
    {
        // do not omit constructor
        void(0);
    };

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override
    {
        auto ptr = req->attributes()->get<std::shared_ptr<std::string>>(
            "test-middleware");
        ptr->append("block");

        mcb(HttpResponse::newHttpResponse());
    }
};

#if defined(__cpp_impl_coroutine)
class MiddlewareCoro : public drogon::HttpCoroMiddleware<MiddlewareCoro>
{
  public:
    MiddlewareCoro()
    {
        // do not omit constructor
        void(0);
    };

    Task<HttpResponsePtr> invoke(const HttpRequestPtr &req,
                                 MiddlewareNextAwaiter &&nextAwaiter) override
    {
        auto ptr = req->attributes()->get<std::shared_ptr<std::string>>(
            "test-middleware");
        ptr->append("coro");

        auto resp = co_await nextAwaiter;

        ptr->append("coro");
        resp->setBody(*ptr);
        co_return resp;
    }
};
#endif

class MiddlewareTest : public drogon::HttpController<MiddlewareTest>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MiddlewareTest::handleRequest,
                  "/test-middleware",
                  Get,
                  "Middleware1",
                  "Middleware2",
                  "Middleware3",
                  "Middleware4");
    ADD_METHOD_TO(MiddlewareTest::handleRequest,
                  "/test-middleware-block",
                  Get,
                  "Middleware1",
                  "Middleware2",
                  "MiddlewareBlock",
                  "Middleware3");

#if defined(__cpp_impl_coroutine)
    ADD_METHOD_TO(MiddlewareTest::handleRequest,
                  "/test-middleware-coro",
                  Get,
                  "Middleware1",
                  "Middleware2",
                  "MiddlewareCoro");
#endif
    METHOD_LIST_END

    void handleRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        req->attributes()
            ->get<std::shared_ptr<std::string>>("test-middleware")
            ->append("test");
        callback(HttpResponse::newHttpResponse());
    }
};
