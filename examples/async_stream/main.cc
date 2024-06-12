#include <drogon/drogon.h>
#include <chrono>
#include <memory>
#include <utility>
#include <fstream>

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
           const StreamContextPtr &streamCtx,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            LOG_INFO << "Headers:";
            for (auto &[k, v] : req->headers())
            {
                LOG_INFO << k << ": " << v;
            }
            LOG_INFO << "Body: " << req->body();

            if (!streamCtx)
            {
                LOG_INFO << "Empty StreamContext, the request does not have a "
                            "body, or stream-mode is not enabled";
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("no stream");
                callback(resp);
                return;
            }

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
                struct File
                {
                    std::string filename;
                    std::ofstream file;
                };

                auto files = std::make_shared<std::vector<File>>();
                handler = HttpStreamHandler::newMultipartHandler(
                    req,
                    [files](MultipartHeader &&header) {
                        LOG_INFO << "Multipart name: " << header.name
                                 << ", filename:" << header.filename
                                 << ", contentType:" << header.contentType;

                        files->emplace_back();
                        files->back().filename = header.filename;
                        files->back().file.open("uploads/" + header.filename,
                                                std::ios::trunc);
                    },
                    [files](const char *data, size_t length) {
                        LOG_INFO << "data[" << length
                                 << "]: " << std::string_view{data, length};
                        if (files->back().file.is_open())
                        {
                            LOG_INFO << "write file";
                            files->back().file.write(data, length);
                            files->back().file.flush();
                        }
                        else
                        {
                            LOG_ERROR << "file not open";
                        }
                    },
                    [files, callback = std::move(callback)]() {
                        LOG_INFO << "stream finish, received " << files->size()
                                 << " files";
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
            streamCtx->setStreamHandler(std::move(handler));
        },
        {Post});

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().setLogLevel(trantor::Logger::kTrace);
    app().enableStreamRequest().addListener("127.0.0.1", 8848).run();
}
