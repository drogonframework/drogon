#include "HttpControllerBinder.h"
#include "HttpResponseImpl.h"

namespace drogon
{

void HttpControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::function<void(const HttpRequestImplPtr &req,
                             const HttpResponsePtr &resp,
                             const std::function<void(const HttpResponsePtr &)>
                                 &callback)> &postHandlingCallback)
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

    auto &paramsVector = req->getRoutingParameters();
    std::deque<std::string> params(paramsVector.size());
    for (int i = 0; i < paramsVector.size(); i++)
    {
        params[i] = paramsVector[i];
    }
    binderPtr_->handleHttpRequest(
        params,
        req,
        // TODO: how to capture postHandlingCallback?
        [this, req, callback = std::move(callback), postHandlingCallback](
            const HttpResponsePtr &resp) {
            if (resp->expiredTime() >= 0 && resp->statusCode() != k404NotFound)
            {
                // cache the response;
                static_cast<HttpResponseImpl *>(resp.get())->makeHeaderString();
                auto loop = req->getLoop();
                if (loop->isInLoopThread())
                {
                    responseCache_.setThreadData(resp);
                }
                else
                {
                    req->getLoop()->queueInLoop(
                        [this, resp]() { responseCache_.setThreadData(resp); });
                }
            }
            postHandlingCallback(req, resp, callback);
        });
}

}  // namespace drogon
