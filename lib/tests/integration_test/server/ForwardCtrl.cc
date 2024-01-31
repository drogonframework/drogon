#include "ForwardCtrl.h"

void ForwardCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    req->setPath("/repos/an-tao/drogon/git/refs/heads/master");
    app().forward(
        req,
        [callback = std::move(callback)](const HttpResponsePtr &resp) {
            callback(resp);
        },
        "https://api.github.com");
}
