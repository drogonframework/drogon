#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
class Client : public drogon::HttpController<Client>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(Client::get, "{1}", Get);
    METHOD_ADD(Client::post, "{1}", Post);
    METHOD_LIST_END

    void get(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback,
             std::string key);
    void post(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback,
              std::string key);
};
