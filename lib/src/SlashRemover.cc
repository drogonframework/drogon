#include "drogon/plugins/SlashRemover.h"
#include "drogon/HttpAppFramework.h"
#include "drogon/utils/FunctionTraits.h"
#include <functional>
#include <string>

using namespace drogon;
using namespace plugin;
using std::string;

#define TRAILING_SLASH_REGEX ".+\\/$"
#define DUPLICATE_SLASH_REGEX ".*\\/{2,}.*"

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

static void redirect(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     const string& newPath)
{
    callback(HttpResponse::newRedirectionResponse(newPath));
}
static void forward(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const string& newPath)
{
    req->setPath(newPath);
    app().forward(req, std::move(callback));
}

static void (*removerFunctions[])(string&) = {
    (void (*)(string&))removeTrailingSlashes,
    removeDuplicateSlashes,
    removeExcessiveSlashes,
};

void SlashRemover::initAndStart(const Json::Value& config)
{
    trailingSlashes_ = config.get("remove_trailing_slashes", true).asBool();
    duplicateSlashes_ = config.get("remove_duplicate_slashes", true).asBool();
    redirect_ = config.get("redirect", true).asBool();
    if (!trailingSlashes_ && !duplicateSlashes_)
        return;
    const uint8_t removerFunctionIndex =
        ((trailingSlashes_ << 0) | (duplicateSlashes_ << 1)) - 1;
    const auto removerFunction = removerFunctions[removerFunctionIndex];
    const auto actionFunction = redirect_ ? redirect : forward;
    app().registerHandlerViaRegex(
        TRAILING_SLASH_REGEX,
        [removerFunction, actionFunction](
            const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& callback) {
            string newPath = req->path();
            removerFunction(newPath);
            actionFunction(req, std::move(callback), newPath);
        });
}

void SlashRemover::shutdown()
{
    LOG_TRACE << "SlashRemover plugin is shutdown!";
}
