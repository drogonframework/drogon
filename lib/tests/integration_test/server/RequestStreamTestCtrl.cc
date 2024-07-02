#include <fstream>
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/RequestStream.h>

using namespace drogon;

class RequestStreamTestCtrl : public HttpController<RequestStreamTestCtrl>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RequestStreamTestCtrl::stream_status, "/stream_status", Get);
    ADD_METHOD_TO(RequestStreamTestCtrl::stream_chunk, "/stream_chunk", Post);
    ADD_METHOD_TO(RequestStreamTestCtrl::stream_upload_echo,
                  "/stream_upload_echo",
                  Post);
    METHOD_LIST_END

    void stream_status(
        const HttpRequestPtr &,
        std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        auto resp = HttpResponse::newHttpResponse();
        if (app().isRequestStreamEnabled())
        {
            resp->setBody("enabled");
        }
        else
        {
            resp->setBody("not enabled");
        }
        callback(resp);
    }

    void stream_chunk(
        const HttpRequestPtr &,
        RequestStreamPtr &&stream,
        std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        if (!stream)
        {
            LOG_INFO << "stream mode is not enabled";
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("no stream");
            callback(resp);
            return;
        }

        auto respBody = std::make_shared<std::string>();
        auto reader = RequestStreamReader::newReader(
            [respBody](const char *data, size_t length) {
                respBody->append(data, length);
            },
            [respBody, callback = std::move(callback)](std::exception_ptr ex) {
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
                    resp->setBody("stream error");
                    callback(resp);
                }
                else
                {
                    resp->setBody(*respBody);
                    callback(resp);
                }
            });
        stream->setStreamReader(std::move(reader));
    }

    void stream_upload_echo(
        const HttpRequestPtr &req,
        RequestStreamPtr &&stream,
        std::function<void(const HttpResponsePtr &)> &&callback) const

    {
        assert(drogon::app().isRequestStreamEnabled() || !stream);

        if (!stream)
        {
            LOG_INFO << "stream mode is not enabled";
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("no stream");
            callback(resp);
            return;
        }
        if (req->contentType() != CT_MULTIPART_FORM_DATA)
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("should upload multipart");
            callback(resp);
            return;
        }

        struct Context
        {
            std::string firstFileContent;
            size_t currentFileIndex_{0};
        };

        auto ctx = std::make_shared<Context>();
        auto reader = RequestStreamReader::newMultipartReader(
            req,
            [ctx](MultipartHeader &&header) { ctx->currentFileIndex_++; },
            [ctx](const char *data, size_t length) {
                if (ctx->currentFileIndex_ == 1)
                {
                    ctx->firstFileContent.append(data, length);
                }
            },
            [ctx, callback = std::move(callback)](std::exception_ptr ex) {
                auto resp = HttpResponse::newHttpResponse();
                if (ex)
                {
                    try
                    {
                        std::rethrow_exception(std::move(ex));
                    }
                    catch (const StreamError &e)
                    {
                        LOG_ERROR << "stream error: " << e.what();
                    }
                    catch (const std::exception &e)
                    {
                        LOG_ERROR << "multipart error: " << e.what();
                    }
                    resp->setStatusCode(k400BadRequest);
                    resp->setBody("error\n");
                    callback(resp);
                }
                else
                {
                    resp->setBody(ctx->firstFileContent);
                    callback(resp);
                }
            });
        stream->setStreamReader(std::move(reader));
    }
};
