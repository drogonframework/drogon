#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <fstream>

using namespace drogon;

class StreamEchoReader : public RequestStreamReader
{
  public:
    StreamEchoReader(ResponseStreamPtr respStream)
        : respStream_(std::move(respStream))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        LOG_INFO << "onStreamData[" << length << "]";
        respStream_->send({data, length});
    }

    void onStreamFinish(std::exception_ptr ptr) override
    {
        if (ptr)
        {
            try
            {
                std::rethrow_exception(ptr);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "onStreamError: " << e.what();
            }
        }
        else
        {
            LOG_INFO << "onStreamFinish";
        }
        respStream_->close();
    }

  private:
    ResponseStreamPtr respStream_;
};

class RequestStreamExampleCtrl : public HttpController<RequestStreamExampleCtrl>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RequestStreamExampleCtrl::stream_echo, "/stream_echo", Post);
    ADD_METHOD_TO(RequestStreamExampleCtrl::stream_upload,
                  "/stream_upload",
                  Post);
    METHOD_LIST_END

    void stream_echo(
        const HttpRequestPtr &,
        RequestStreamPtr &&stream,
        std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        auto resp = drogon::HttpResponse::newAsyncStreamResponse(
            [stream](ResponseStreamPtr respStream) {
                stream->setStreamReader(
                    std::make_shared<StreamEchoReader>(std::move(respStream)));
            });
        callback(resp);
    }

    void stream_upload(
        const HttpRequestPtr &req,
        RequestStreamPtr &&stream,
        std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        struct Entry
        {
            MultipartHeader header;
            std::string tmpName;
            std::ofstream file;
        };

        auto files = std::make_shared<std::vector<Entry>>();
        auto reader = RequestStreamReader::newMultipartReader(
            req,
            [files](MultipartHeader &&header) {
                LOG_INFO << "Multipart name: " << header.name
                         << ", filename:" << header.filename
                         << ", contentType:" << header.contentType;

                files->push_back({std::move(header)});
                auto tmpName = drogon::utils::genRandomString(40);
                if (!files->back().header.filename.empty())
                {
                    files->back().tmpName = tmpName;
                    files->back().file.open("uploads/" + tmpName,
                                            std::ios::trunc);
                }
            },
            [files](const char *data, size_t length) {
                if (files->back().tmpName.empty())
                {
                    return;
                }
                auto &currentFile = files->back().file;
                if (length == 0)
                {
                    LOG_INFO << "file finish";
                    if (currentFile.is_open())
                    {
                        currentFile.flush();
                        currentFile.close();
                    }
                    return;
                }
                LOG_INFO << "data[" << length << "]: ";
                if (currentFile.is_open())
                {
                    LOG_INFO << "write file";
                    currentFile.write(data, length);
                }
                else
                {
                    LOG_ERROR << "file not open";
                }
            },
            [files, callback = std::move(callback)](std::exception_ptr ex) {
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
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k400BadRequest);
                    resp->setBody("error\n");
                    callback(resp);
                }
                else
                {
                    LOG_INFO << "stream finish, received " << files->size()
                             << " files";
                    Json::Value respJson;
                    for (const auto &item : *files)
                    {
                        if (item.tmpName.empty())
                            continue;
                        Json::Value entry;
                        entry["name"] = item.header.name;
                        entry["filename"] = item.header.filename;
                        entry["tmpName"] = item.tmpName;
                        respJson.append(entry);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(respJson);
                    callback(resp);
                }
            });

        stream->setStreamReader(std::move(reader));
    }
};
