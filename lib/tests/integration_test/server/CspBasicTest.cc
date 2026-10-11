#include "CspBasicTest.h"

void CspBasicTest::delimiterEscaping(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    drogon::HttpViewData data;
    data["%>"] = std::string("hello");
    data["%}"] = std::string("world");
    data["test]]more"] = std::string("!!!");
    auto res =
        drogon::HttpResponse::newHttpViewResponse("DelimiterEscaping", data);
    callback(res);
}

void CspBasicTest::stringLiteralEscaping(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    drogon::HttpViewData data;
    data["a\"bc"] = std::string("hello");
    data["abc\\"] = std::string("world");
    data["abc\\\""] = std::string("!!!");
    auto res =
        drogon::HttpResponse::newHttpViewResponse("StringLiteralEscaping",
                                                  data);
    callback(res);
}

void CspBasicTest::layoutAndViewRendering(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    drogon::HttpViewData data;
    // used by sub_view
    data["a"] = std::string("hello");
    auto res = drogon::HttpResponse::newHttpViewResponse("LayoutAndView", data);
    callback(res);
}

void CspBasicTest::layoutViewStringEscaping(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto res =
        drogon::HttpResponse::newHttpViewResponse("LayoutViewStringEscaping");
    callback(res);
}
