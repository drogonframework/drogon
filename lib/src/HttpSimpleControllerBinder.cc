#include "HttpSimpleControllerBinder.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpSimpleController.h>

namespace drogon
{

void HttpSimpleControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Binders without controller should be removed after run()
    assert(controller_);

    try
    {
        auto cb = callback;  // copy
        controller_->asyncHandleHttpRequest(req, std::move(cb));
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
}

}  // namespace drogon
