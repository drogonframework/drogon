#include <drogon/HttpController.h>

using namespace drogon;

// HttpControllers are automatically added to Drogon upon Drogon initializing.
class SayHello : public HttpController<Wells>
{
  public:
    METHOD_LIST_BEGIN
    // Drogon automatically appends the namespace and name of the controller to
    // the handlers of the controller. In this example, although we are adding
    // a handler to /. But because it is a part of the SayHello controller. It
    // ends up in path /SayHello/ (IMPORTANT! It is /SayHello/ not /SayHello
    // as they are different paths).
    METHOD_ADD(Wells::genericDisplay, "/", Get);
    // Same for /hello. It ends up at /SayHello/hello
    METHOD_ADD(Wells::personalizedModify, "/modify", Get);
    METHOD_LIST_END

  protected:
    void genericDisplay(const HttpRequestPtr &,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(
            "Wells will go HERE"
            "controller");
        callback(resp);
    }

    void personalizedModify(
        const HttpRequestPtr &,
        std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(
            "Modify Wells HERE");
        callback(resp);
    }
};
