#include <drogon/HttpSimpleController.h>
#include <drogon/HttpResponse.h>

using namespace drogon;

// HttpSimpleController does not allow registration of multiple handlers.
// Instead, it has one handler - asyncHandleHttpRequest. The
// HttpSimpleController is a lightweight class designed to handle really simple
// cases.
class HelloViewController : public HttpSimpleController<HelloViewController>
{
  public:
    PATH_LIST_BEGIN
    // Also unlike HttpContoller, HttpSimpleController does not automatically
    // prepend the namespace and class name to the path. Thus the path of this
    // controller is at "/view".
    PATH_ADD("/view");
    PATH_LIST_END

    void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override
    {
        HttpViewData data;
        data["name"] = req->getParameter("name");
        auto resp = HttpResponse::newHttpViewResponse("HelloView", data);
        callback(resp);
    }
};