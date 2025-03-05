#include <drogon/drogon.h>
#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <trantor/utils/Logger.h>
#include <trantor/net/callbacks.h>
#include <trantor/net/TcpConnection.h>

using namespace drogon;
using namespace std::chrono_literals;

std::mutex mutex;
std::unordered_map<trantor::TcpConnectionPtr, std::function<void()>>
    connMapping;

int main()
{
    app().registerHandler(
        "/stream",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto &weakConnPtr = req->getConnectionPtr();
            if (auto connPtr = weakConnPtr.lock())
            {
                std::lock_guard lk(mutex);
                connMapping.emplace(std::move(connPtr), [] {
                    LOG_INFO << "call stop or other options!!!!";
                });
            }
            auto resp = drogon::HttpResponse::newAsyncStreamResponse(
                [](drogon::ResponseStreamPtr stream) {
                    std::thread([stream =
                                     std::shared_ptr<drogon::ResponseStream>{
                                         std::move(stream)}]() mutable {
                        std::cout << std::boolalpha << stream->send("hello ")
                                  << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        std::cout << std::boolalpha << stream->send("world");
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        stream->close();
                    }).detach();
                });
            resp->setContentTypeCodeAndCustomString(
                ContentType::CT_APPLICATION_JSON, "application/json");
            callback(resp);
        });

    // Example: register a stream-mode function handler
    app().registerHandler(
        "/stream_req",
        [](const HttpRequestPtr &req,
           RequestStreamPtr &&stream,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            if (!stream)
            {
                LOG_INFO << "stream mode is not enabled";
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("no stream");
                callback(resp);
                return;
            }

            auto reader = RequestStreamReader::newReader(
                [](const char *data, size_t length) {
                    LOG_INFO << "piece[" << length
                             << "]: " << std::string_view{data, length};
                },
                [callback = std::move(callback)](std::exception_ptr ex) {
                    auto resp = HttpResponse::newHttpResponse();
                    if (ex)
                    {
                        try
                        {
                            std::rethrow_exception(std::move(ex));
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR << "stream error: " << e.what();
                        }
                        resp->setStatusCode(k400BadRequest);
                        resp->setBody("error\n");
                        callback(resp);
                    }
                    else
                    {
                        LOG_INFO << "stream finish";
                        resp->setBody("success\n");
                        callback(resp);
                    }
                });

            stream->setStreamReader(std::move(reader));
        },
        {Post});

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().enableRequestStream();  // This is for request stream.
    app().setConnectionCallback([](const trantor::TcpConnectionPtr &conn) {
        if (conn->disconnected())
        {
            std::lock_guard lk(mutex);
            if (auto it = connMapping.find(conn); it != connMapping.end())
            {
                LOG_INFO << "disconnect";
                connMapping[conn]();
                connMapping.erase(conn);
            }
        }
    });
    app().addListener("127.0.0.1", 8848).run();
}
