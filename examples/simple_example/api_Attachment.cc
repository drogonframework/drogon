#include "api_Attachment.h"
#include <fstream>

using namespace api;
//add definition of your processing function here
void Attachment::get(const HttpRequestPtr &req,
                     const std::function<void(const HttpResponsePtr &)> &callback)
{
    auto resp = HttpResponse::newHttpViewResponse("FileUpload", HttpViewData());
    callback(resp);
}

void Attachment::upload(const HttpRequestPtr &req,
                        const std::function<void(const HttpResponsePtr &)> &callback)
{
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) == 0)
    {
        //LOG_DEBUG << "upload good!";
        auto files = fileUpload.getFiles();
        //LOG_DEBUG << "file num=" << files.size();
        for (auto const &file : files)
        {
            LOG_DEBUG << "file:"
                      << file.getFileName()
                      << "(len="
                      << file.fileLength()
                      << ",md5="
                      << file.getMd5()
                      << ")";
            file.save();
            file.save("123");
            file.saveAs("456/hehe");
            file.saveAs("456/7/8/9/" + file.getMd5());
            file.save("..");
            file.save(".xx");
            file.saveAs("../xxx");
        }
        Json::Value json;
        json["result"] = "ok";
        for (auto &param : fileUpload.getParameters())
        {
            json[param.first] = param.second;
        }
        auto resp = HttpResponse::newHttpJsonResponse(json);
        callback(resp);
        return;
    }
    LOG_DEBUG << "upload error!";
    //LOG_DEBUG<<req->con
    Json::Value json;
    json["result"] = "failed";
    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
}

void Attachment::download(const HttpRequestPtr &req,
                          const std::function<void(const HttpResponsePtr &)> &callback)
{
    auto resp = HttpResponse::newFileResponse("./drogon.jpg", "", CT_IMAGE_JPG);
    callback(resp);
}
