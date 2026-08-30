#include "PromTestCtrl.h"

using namespace drogon;

void PromTestCtrl::fast(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Hello, world!");
    callback(resp);
}

drogon::AsyncTask PromTestCtrl::slow(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback)
{
    // sleep for a random time between 1 and 3 seconds
    static std::once_flag flag;
    std::call_once(flag, []() { srand(time(nullptr)); });
    auto duration = 1 + (rand() % 3);
    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
    co_await drogon::sleepCoro(loop, std::chrono::seconds(duration));
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Hello, world!");
    callback(resp);
    co_return;
}
