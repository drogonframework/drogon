#pragma once

#include <drogon/HttpController.h>
using namespace drogon;

class CspBasicTest : public drogon::HttpController<CspBasicTest>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(CspBasicTest::delimiterEscaping, "/delimiter-escaping", Get);
    METHOD_ADD(CspBasicTest::stringLiteralEscaping,
               "/string-literal-escaping",
               Get);
    METHOD_ADD(CspBasicTest::layoutAndViewRendering, "/layout-and-view", Get);
    METHOD_ADD(CspBasicTest::layoutViewStringEscaping,
               "/layout-view-string-escaping",
               Get);
    METHOD_LIST_END

    void delimiterEscaping(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void stringLiteralEscaping(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void layoutAndViewRendering(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void layoutViewStringEscaping(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
};
