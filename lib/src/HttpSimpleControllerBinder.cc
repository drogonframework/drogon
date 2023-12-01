#include "HttpSimpleControllerBinder.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpSimpleController.h>

namespace drogon
{

void HttpSimpleControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto &controller = controller_;
    if (controller)
    {
        try
        {
            auto cb = callback;  // copy
            controller->asyncHandleHttpRequest(req, std::move(cb));
        }
        catch (const std::exception &e)
        {
            app().getExceptionHandler()(e, req, std::move(callback));
            return;
        }
        catch (...)
        {
            LOG_ERROR << "Exception not derived from std::exception";
            return;
        }

        return;
    }
    else
    {
        // TODO: should we return 404 in the first place?
        LOG_ERROR << "can't find controller " << handlerName_;
        auto res = drogon::HttpResponse::newNotFoundResponse(req);
        callback(res);
    }
}

}  // namespace drogon
