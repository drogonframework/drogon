#pragma once
#include <drogon/HttpApiController.h>
using namespace drogon;
namespace api
{
class Attachment : public drogon::HttpApiController<Attachment>
{
  public:
    METHOD_LIST_BEGIN
    //use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(Attachment::get, "/", "drogon::GetFilter");
    METHOD_ADD(Attachment::upload, "/upload", "drogon::PostFilter");

    METHOD_LIST_END
    //your declaration of processing function maybe like this:
    void get(const HttpRequestPtr &req,
             const std::function<void(const HttpResponsePtr &)> &callback);
    void upload(const HttpRequestPtr &req,
                const std::function<void(const HttpResponsePtr &)> &callback);
};
} // namespace api
