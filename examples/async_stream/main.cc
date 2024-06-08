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

    app().registerHandler(
        "/stream_req",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            LOG_INFO << "Headers:";
            for (auto &[k, v] : req->headers())
            {
                LOG_INFO << k << ": " << v;
            }
            LOG_INFO << "Body: " << req->body();

            HttpStreamHandlerPtr handler;
            LOG_INFO << "ContentTypeCode: " << req->contentType();
            if (req->contentType() != CT_MULTIPART_FORM_DATA)
            {
                handler = HttpStreamHandler::newHandler(
                    [](const char *data, size_t length) {
                        LOG_INFO << "piece[" << length
                                 << "]: " << std::string_view{data, length};
                    },
                    [callback = std::move(callback)]() {
                        LOG_INFO << "stream finish";
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setBody("success\n");
                        callback(resp);
                    },
                    [](std::exception_ptr ex) {
                        try
                        {
                            std::rethrow_exception(std::move(ex));
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR << "stream error: " << e.what();
                        }
                    });
            }
            else
            {
                handler = HttpStreamHandler::newMultipartHandler(
                    req,
                    [](MultipartHeader &&header) {
                        LOG_INFO << "Multipart name: " << header.name
                                 << ", filename:" << header.filename
                                 << ", contentType:" << header.contentType;
                    },
                    [](const char *data, size_t length) {
                        LOG_INFO << "data[" << length
                                 << "]: " << std::string_view{data, length};
                    },
                    [callback = std::move(callback)]() {
                        LOG_INFO << "stream finish";
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setBody("success\n");
                        callback(resp);
                    },
                    [](std::exception_ptr ex) {
                        try
                        {
                            std::rethrow_exception(std::move(ex));
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR << "stream error: " << e.what();
                        }
                    });
            }
            req->setStreamHandler(std::move(handler));
        },
        {Post});

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().enableStreamRequest().addListener("127.0.0.1", 8848).run();
}
