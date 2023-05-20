#include "drogon/plugins/SlashRemover.h"
#include "drogon/HttpAppFramework.h"
#include "drogon/utils/FunctionTraits.h"
#include <functional>
#include <string>

using namespace drogon;
using namespace plugin;
using std::string;

#define TRAILING_SLASH_REGEX ".+\\/$"
#define DUPLICATE_SLASH_REGEX ".*\\/{2}.*"

static bool removeTrailingSlashes(string& url)
{
    // If only contains '/', it will return -1, and added with 1 it will be 0,
    // hence empty, meaning root
    url.resize(url.find_last_not_of('/') + 1);
    if (url.empty())  // Root
    {
        url = '/';
        return true;
    }
    return false;
}
static void removeDuplicateSlashes(string& url)
{
    size_t a = 0;
    for (size_t b = 1, len = url.size(); b < len; ++b)
    {
        const char c = url[b];
        if (c != '/' || url[a] != '/')
            url[++a] = c;
    }
    url.resize(a + 1);
}
static void removeExcessiveSlashes(string& url)
{
    if (url[url.size() - 1] ==
            '/' &&  // This check is so we don't search if there is no trailing
                    // slash to begin with
        removeTrailingSlashes(
            url))  // If it is root path, we don't need to check for duplicates
        return;

    removeDuplicateSlashes(url);
}

static void trailingSlashRemover(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeTrailingSlashes(redirectedPath);
    callback(HttpResponse::newRedirectionResponse(redirectedPath));
}
static void trailingSlashRemoverForward(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeTrailingSlashes(redirectedPath);
    req->setPath(redirectedPath);
    app().forward(req, std::move(callback));
}
static void duplicateSlashRemover(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeDuplicateSlashes(redirectedPath);
    callback(HttpResponse::newRedirectionResponse(redirectedPath));
}
static void duplicateSlashRemoverForward(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeDuplicateSlashes(redirectedPath);
    req->setPath(redirectedPath);
    app().forward(req, std::move(callback));
}
static void excessiveSlashRemover(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeExcessiveSlashes(redirectedPath);
    callback(HttpResponse::newRedirectionResponse(redirectedPath));
}
static void excessiveSlashRemoverForward(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    string redirectedPath = req->path();
    removeExcessiveSlashes(redirectedPath);
    req->setPath(redirectedPath);
    app().forward(req, std::move(callback));
}

void SlashRemover::initAndStart(const Json::Value& config)
{
    trailingSlashes_ = config.get("remove_trailing_slashes", true).asBool();
    duplicateSlashes_ = config.get("remove_duplicate_slashes", true).asBool();
    redirect_ = config.get("redirect", true).asBool();
    switch ((trailingSlashes_ << 0) | (duplicateSlashes_ << 1))
    {
        case 1 << 0:
        {
            app().registerHandlerViaRegex(
                TRAILING_SLASH_REGEX,
                [this](const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
                    if (redirect_)
                        trailingSlashRemover(req, std::move(callback));
                    else
                        trailingSlashRemoverForward(req, std::move(callback));
                });
            break;
        }
        case 1 << 1:
        {
            app().registerHandlerViaRegex(
                DUPLICATE_SLASH_REGEX,
                [this](const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
                    if (redirect_)
                        duplicateSlashRemover(req, std::move(callback));
                    else
                        duplicateSlashRemoverForward(req, std::move(callback));
                });
            break;
        }
        case (1 << 0) | (1 << 1):
        {
            app().registerHandlerViaRegex(
                TRAILING_SLASH_REGEX "|" DUPLICATE_SLASH_REGEX,
                [this](const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
                    if (redirect_)
                        excessiveSlashRemover(req, std::move(callback));
                    else
                        excessiveSlashRemoverForward(req, std::move(callback));
                });
            break;
        }
        default:
            return;
    }
}

void SlashRemover::shutdown()
{
    LOG_TRACE << "SlashRemover plugin is shutdown!";
}
