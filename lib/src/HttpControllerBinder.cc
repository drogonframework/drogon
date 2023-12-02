#include "HttpControllerBinder.h"
#include "HttpResponseImpl.h"

namespace drogon
{

void HttpControllerBinder::handleRequest(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto &paramsVector = req->getRoutingParameters();
    std::deque<std::string> params(paramsVector.begin(), paramsVector.end());
    binderPtr_->handleHttpRequest(params, req, std::move(callback));
}

}  // namespace drogon
