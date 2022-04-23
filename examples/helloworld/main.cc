#include <drogon/drogon.h>
using namespace drogon;

int main()
{
    // `registerHandler()` adds a handler to the desired path. The handler is
    // responsible for generating a HTTP response upon an HTTP request being
    // sent to Drogon
    app().registerHandler(
        "/",
        [](const HttpRequestPtr &,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("Hello, World!");
            callback(resp);
        },
        {Get});

    // `registerHandler()` also supports parsing and passing the path as
    // parameters to the handler. Parameters are specified using {}. The text
    // inside the {} does not correspond to the index of parameter passed to the
    // handler (nor it has any meaning). Instead, it is only to make it easier
    // for users to recognize the function of each parameter.
    app().registerHandler(
        "/user/{user-name}",
        [](const HttpRequestPtr &,
           std::function<void(const HttpResponsePtr &)> &&callback,
           const std::string &name) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("Hello, " + name + "!");
            callback(resp);
        },
        {Get});

    // You can also specify that the parameter is in the query section of the
    // URL!
    app().registerHandler(
        "/hello?user={user-name}",
        [](const HttpRequestPtr &,
           std::function<void(const HttpResponsePtr &)> &&callback,
           const std::string &name) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("Hello, " + name + "!");
            callback(resp);
        },
        {Get});

    // Or, if you want to, instead of asking drogon to parse it for you. You can
    // parse the request yourselves.
    app().registerHandler(
        "/hello_user",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            auto name = req->getOptionalParameter<std::string>("user");
            if (!name)
                resp->setBody("Please tell me your name");
            else
                resp->setBody("Hello, " + name.value() + "!");
            callback(resp);
        },
        {Get});

    // Ask Drogon to listen on 127.0.0.1 port 8848. Drogon supports listening
    // on multiple IP addresses by adding multiple listeners. For example, if
    // you want the server also listen on 127.0.0.1 port 5555. Just add another
    // line of addListener("127.0.0.1", 5555)
    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().addListener("127.0.0.1", 8848).run();
}
