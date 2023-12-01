#include "HttpSimpleControllerBinder.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpSimpleController.h>

namespace drogon
{

void HttpSimpleControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::function<void(const HttpRequestImplPtr &req,
                             const HttpResponsePtr &resp,
                             const std::function<void(const HttpResponsePtr &)>
                                 &callback)> &postHandlingCallback)
{
    auto &controller = controller_;
    if (controller)
    {
        auto &responsePtr = *responseCache_;
        if (responsePtr)
        {
            if (responsePtr->expiredTime() == 0 ||
                (trantor::Date::now() <
                 responsePtr->creationDate().after(
                     static_cast<double>(responsePtr->expiredTime()))))
            {
                // use cached response!
                LOG_TRACE << "Use cached response";
                postHandlingCallback(req, responsePtr, callback);
                return;
            }
            else
            {
                responsePtr.reset();
            }
        }
        try
        {
            controller->asyncHandleHttpRequest(
                req,
                // TODO: how to capture postHandlingCallback?
                [this, req, callback, postHandlingCallback](
                    const HttpResponsePtr &resp) {
                    auto newResp = resp;
                    if (resp->expiredTime() >= 0 &&
                        resp->statusCode() != k404NotFound)
                    {
                        // cache the response;
                        static_cast<HttpResponseImpl *>(resp.get())
                            ->makeHeaderString();
                        auto loop = req->getLoop();

                        if (loop->isInLoopThread())
                        {
                            responseCache_.setThreadData(resp);
                        }
                        else
                        {
                            loop->queueInLoop([this, resp]() {
                                responseCache_.setThreadData(resp);
                            });
                        }
                    }
                    postHandlingCallback(req, newResp, callback);
                });
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
        postHandlingCallback(req, res, callback);
    }
}

}  // namespace drogon
