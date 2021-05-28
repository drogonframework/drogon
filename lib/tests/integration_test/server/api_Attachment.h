#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
namespace api
{
class Attachment : public drogon::HttpController<Attachment>
{
  public:
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(Attachment::get, "", Get);  // Path is '/api/attachment'
    METHOD_ADD(Attachment::upload, "/upload", Post);
    METHOD_ADD(Attachment::uploadImage, "/uploadImage", Post);
    METHOD_ADD(Attachment::download, "/download", Get);
    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    void get(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback);
    void upload(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);
    void uploadImage(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    void download(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
};
}  // namespace api
