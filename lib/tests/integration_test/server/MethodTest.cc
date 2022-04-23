#include "MethodTest.h"
static void makeGetRespose(
    const std::function<void(const HttpResponsePtr &)> &callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse("GET"));
}

static void makePostRespose(
    const std::function<void(const HttpResponsePtr &)> &callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse("POST"));
}

void MethodTest::get(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
{
    LOG_DEBUG;
    makeGetRespose(callback);
}
void MethodTest::post(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      std::string str)
{
    LOG_DEBUG << str;
    makePostRespose(callback);
}

void MethodTest::getReg(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        std::string regStr)
{
    LOG_DEBUG << regStr;
    makeGetRespose(callback);
}
void MethodTest::postReg(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string regStr,
    std::string str)
{
    LOG_DEBUG << regStr;
    LOG_DEBUG << str;
    makePostRespose(callback);
}

void MethodTest::postRegex(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string regStr)
{
    LOG_DEBUG << regStr;
    makePostRespose(callback);
}