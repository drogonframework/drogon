#include "api_Attachment.h"
#include <fstream>

using namespace api;
// add definition of your processing function here
void Attachment::get(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpViewResponse("FileUpload", HttpViewData());
    callback(resp);
}

void Attachment::upload(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) == 0)
    {
        // LOG_DEBUG << "upload good!";
        auto &files = fileUpload.getFiles();
        // LOG_DEBUG << "file num=" << files.size();
        for (auto const &file : files)
        {
            LOG_DEBUG << "file:" << file.getFileName()
                      << "(extension=" << file.getFileExtension()
                      << ",type=" << file.getFileType()
                      << ",len=" << file.fileLength()
                      << ",md5=" << file.getMd5() << ")";
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
    // LOG_DEBUG << req->con
    Json::Value json;
    json["result"] = "failed";
    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
}

void Attachment::uploadImage(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    MultiPartParser fileUpload;

    // At this endpoint, we only accept one file
    if (fileUpload.parse(req) == 0 && fileUpload.getFiles().size() == 1)
    {
        // LOG_DEBUG << "upload image good!";
        Json::Value json;

        // Get the first file received
        auto &file = fileUpload.getFiles()[0];

        // There are 2 ways to check if the file extension is an image.
        // First way
        if (file.getFileType() == FT_IMAGE)
        {
            json["isImage"] = true;
        }
        // Second way
        auto fileExtension = file.getFileExtension();
        if (fileExtension == "png" || fileExtension == "jpeg" ||
            fileExtension == "jpg" || fileExtension == "ico" /* || etc... */)
        {
            json["isImage"] = true;
        }
        else
        {
            json["isImage"] = false;
        }

        json["result"] = "ok";
        for (auto &param : fileUpload.getParameters())
        {
            json[param.first] = param.second;
        }
        auto resp = HttpResponse::newHttpJsonResponse(json);
        callback(resp);
        return;
    }
    LOG_DEBUG << "upload image error!";
    // LOG_DEBUG << req->con
    Json::Value json;
    json["result"] = "failed";
    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
}

void Attachment::download(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newFileResponse("./drogon.jpg", "", CT_IMAGE_JPG);
    callback(resp);
}
