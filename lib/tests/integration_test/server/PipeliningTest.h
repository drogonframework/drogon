#pragma once
#include <drogon/HttpController.h>
using namespace drogon;

// class PipeliningTest : public drogon::HttpSimpleController<PipeliningTest>
//{
//   public:
//     virtual void asyncHandleHttpRequest(
//         const HttpRequestPtr &req,
//         std::function<void(const HttpResponsePtr &)> &&callback) override;
//     PATH_LIST_BEGIN
//     // list path definitions here;
//     PATH_ADD("/pipe", Get);
//     PATH_LIST_END
// };

class PipeliningTest : public drogon::HttpController<PipeliningTest>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PipeliningTest::normalPipe, "/pipe", Get);
    ADD_METHOD_TO(PipeliningTest::strangePipe1, "/pipe/strange-1", Get);
    ADD_METHOD_TO(PipeliningTest::strangePipe2, "/pipe/strange-2", Get);
    METHOD_LIST_END

    void normalPipe(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void strangePipe1(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    void strangePipe2(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
};
