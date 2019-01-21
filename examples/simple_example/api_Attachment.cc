#include "api_Attachment.h"
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
    FileUpload fileUpload;
    fileUpload.parse(req);
    auto files = fileUpload.getFiles();
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
        file.saveAs("456/7/8/9/"+file.getMd5());
        file.save("..");
        file.save(".xx");
        file.saveAs("../xxx");
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpResponse::k200OK);
    callback(resp);
}
