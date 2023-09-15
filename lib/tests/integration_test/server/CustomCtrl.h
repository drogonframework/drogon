#pragma once
#include <drogon/HttpController.h>
using namespace drogon;

class CustomCtrl : public drogon::HttpController<CustomCtrl, false>
{
  public:
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(CustomCtrl::hello,
               "/{userName}",
               Get,
               "CustomHeaderFilter");  // path is /customctrl/{arg1}
    METHOD_LIST_END

    explicit CustomCtrl(const std::string &greetings) : greetings_(greetings)
    {
    }

    void hello(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               const std::string &userName) const;

  private:
    std::string greetings_;
};
