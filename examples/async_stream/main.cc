#include <drogon/drogon.h>
#include <chrono>
#include <memory>
using namespace drogon;
using namespace std::chrono_literals;

int main()
{
    app().registerHandler(
        "/stream",
        [](const HttpRequestPtr &,
           std::function<void(const HttpResponsePtr &)> &&callback) {
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

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().addListener("127.0.0.1", 8848).run();
}
