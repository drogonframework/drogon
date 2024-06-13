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

    class StreamEchoHandler : public RequestStreamHandler
    {
      public:
        StreamEchoHandler(ResponseStreamPtr respStream)
            : respStream_(std::move(respStream))
        {
        }

        void onStreamData(const char *data, size_t length) override
        {
            LOG_INFO << "onStreamData[" << length << "]";
            respStream_->send({data, length});
        }

        void onStreamFinish() override
        {
            LOG_INFO << "onStreamFinish";
            respStream_->close();
        }

        void onStreamError(std::exception_ptr ptr) override
        {
            try
            {
                std::rethrow_exception(ptr);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "onStreamError: " << e.what();
            }
            respStream_->close();
        }

      private:
        ResponseStreamPtr respStream_;
    };

    app().registerHandler(
        "/stream_echo",
        [](const HttpRequestPtr &req,
           RequestStreamPtr &&stream,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newAsyncStreamResponse(
                [stream](ResponseStreamPtr respStream) {
                    auto reqStreamHandler = std::make_shared<StreamEchoHandler>(
                        std::move(respStream));
                    stream->setStreamHandler(reqStreamHandler);
                });
            callback(resp);
        },
        {Get, Post});

    app().registerHandler(
        "/stream_req",
        [](const HttpRequestPtr &req,
           RequestStreamPtr &&stream,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            LOG_INFO << "Headers:";
            for (auto &[k, v] : req->headers())
            {
                LOG_INFO << k << ": " << v;
            }
            LOG_INFO << "Body: " << req->body();

            if (!stream)
            {
                LOG_INFO << "Empty RequestStream, the request does not have a "
                            "body, or stream-mode is not enabled";
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("no stream");
                callback(resp);
                return;
            }

            auto callbackPtr =
                std::make_shared<std::decay_t<decltype(callback)>>(
                    std::move(callback));
            RequestStreamHandlerPtr handler;
            LOG_INFO << "ContentTypeCode: " << req->contentType();
            if (req->contentType() != CT_MULTIPART_FORM_DATA)
            {
                handler = RequestStreamHandler::newHandler(
                    [](const char *data, size_t length) {
                        LOG_INFO << "piece[" << length
                                 << "]: " << std::string_view{data, length};
                    },
                    [callbackPtr]() {
                        LOG_INFO << "stream finish";
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setBody("success\n");
                        (*callbackPtr)(resp);
                    },
                    [callbackPtr](std::exception_ptr ex) {
                        try
                        {
                            std::rethrow_exception(std::move(ex));
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR << "stream error: " << e.what();
                        }
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k400BadRequest);
                        resp->setBody("error\n");
                        (*callbackPtr)(resp);
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
                handler = RequestStreamHandler::newMultipartHandler(
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
                    [files, callbackPtr]() {
                        LOG_INFO << "stream finish, received " << files->size()
                                 << " files";
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setBody("success\n");
                        (*callbackPtr)(resp);
                    },
                    [callbackPtr](std::exception_ptr ex) {
                        try
                        {
                            std::rethrow_exception(std::move(ex));
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR << "stream error: " << e.what();
                        }
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k400BadRequest);
                        resp->setBody("error\n");
                        (*callbackPtr)(resp);
                    });
            }
            stream->setStreamHandler(std::move(handler));
        },
        {Post});

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app().enableStreamRequest().addListener("127.0.0.1", 8848).run();
}
