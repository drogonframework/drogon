#include "PipeliningTest.h"
#include <trantor/net/EventLoop.h>
#include <atomic>
#include <mutex>

void PipeliningTest::normalPipe(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    static std::atomic<int> counter{0};
    int c = counter.fetch_add(1);
    if (c % 3 == 1)
    {
        auto resp = HttpResponse::newHttpResponse();
        auto str = utils::formattedString("<P>the %dth response</P>", c);
        resp->addHeader("counter", utils::formattedString("%d", c));
        resp->setBody(std::move(str));
        callback(resp);
        return;
    }
    double delay = ((double)(10 - (c % 10))) / 10.0;
    if (c % 3 == 2)
    {
        // call the callback in another thread.
        drogon::app().getLoop()->runAfter(delay, [c, callback]() {
            auto resp = HttpResponse::newHttpResponse();
            auto str = utils::formattedString("<P>the %dth response</P>", c);
            resp->addHeader("counter", utils::formattedString("%d", c));
            resp->setBody(std::move(str));
            callback(resp);
        });
        return;
    }
    trantor::EventLoop::getEventLoopOfCurrentThread()->runAfter(
        delay, [c, callback]() {
            auto resp = HttpResponse::newHttpResponse();
            auto str = utils::formattedString("<P>the %dth response</P>", c);
            resp->addHeader("counter", utils::formattedString("%d", c));
            resp->setBody(std::move(str));
            callback(resp);
        });
}

// Receive 1, cache 1
// Receive 2, send 1 send 2
void PipeliningTest::strangePipe1(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    static std::mutex mtx;
    static std::vector<
        std::pair<std::function<void(const HttpResponsePtr &)>, std::string>>
        callbacks;

    LOG_INFO << "Receive request " << req->body();
    std::function<void(const HttpResponsePtr &)> cb1;
    std::string body1;
    std::function<void(const HttpResponsePtr &)> cb2;
    std::string body2;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (callbacks.empty())
        {
            callbacks.emplace_back(std::move(callback), req->getBody());
            return;
        }
        auto item = std::move(callbacks.back());
        callbacks.pop_back();
        cb1 = std::move(item.first);
        body1 = std::move(item.second);
        cb2 = std::move(callback);
        body2 = std::string{req->body()};
    }
    if (cb1)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(body1);
        cb1(resp);
    }
    if (cb2)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(body2);
        cb2(resp);
    }
}

// Receive 1, cache 1
// Receive 2, send 1 cache 2
// Receive 3, send 2 send 3
void PipeliningTest::strangePipe2(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    static std::mutex mtx;
    static std::vector<
        std::pair<std::function<void(const HttpResponsePtr &)>, std::string>>
        callbacks;
    static uint64_t idx{0};

    LOG_INFO << "Receive request " << req->body();
    std::function<void(const HttpResponsePtr &)> cb1;
    std::string body1;
    std::function<void(const HttpResponsePtr &)> cb2;
    std::string body2;
    {
        std::lock_guard<std::mutex> lock(mtx);
        ++idx;
        if (idx % 3 == 1)
        {
            assert(callbacks.empty());
            callbacks.emplace_back(std::move(callback), req->getBody());
            return;
        }
        assert(callbacks.size() == 1);
        if (idx % 3 == 2)
        {
            auto item = std::move(callbacks.back());
            cb1 = std::move(item.first);
            body1 = std::move(item.second);
            callbacks.pop_back();
            callbacks.emplace_back(std::move(callback), req->getBody());
        }
        else
        {
            auto item = std::move(callbacks.back());
            cb1 = std::move(item.first);
            body1 = std::move(item.second);
            callbacks.pop_back();
            cb2 = std::move(callback);
            body2 = std::string{req->body()};
        }
    }
    if (cb1)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(body1);
        cb1(resp);
    }
    if (cb2)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(body2);
        cb2(resp);
    }
}
