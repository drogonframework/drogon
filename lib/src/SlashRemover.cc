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

static inline bool removeTrailingSlashes(string& url)
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
static inline void removeDuplicateSlashes(string& url)
{
    size_t a = 1, len = url.size();
    for (; a < len && (url[a - 1] != '/' || url[a] != '/'); ++a)
    {
    }
    for (size_t b = --a + 1; b < len; ++b)
    {
        const char c = url[b];
        if (c != '/' || url[a] != '/')
            url[++a] = c;
    }
    url.resize(a + 1);
}
static inline void removeExcessiveSlashes(string& url)
{
    if (url[url.size() - 1] ==
            '/' &&  // This check is so we don't search if there is no trailing
                    // slash to begin with
        removeTrailingSlashes(
            url))  // If it is root path, we don't need to check for duplicates
        return;

    removeDuplicateSlashes(url);
}

void SlashRemover::initAndStart(const Json::Value& config)
{
    trailingSlashes_ = config.get("remove_trailing_slashes", true).asBool();
    duplicateSlashes_ = config.get("remove_duplicate_slashes", true).asBool();
    redirect_ = config.get("redirect", true).asBool();
    const uint8_t removerFunctionMask =
        (trailingSlashes_ << 0) | (duplicateSlashes_ << 1);
    if (!removerFunctionMask)
        return;
    app().registerHandlerViaRegex(
        TRAILING_SLASH_REGEX,
        [removerFunctionMask,
         this](const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback) {
            string newPath = req->path();
            switch (removerFunctionMask)
            {
                case 1 << 0:
                    removeTrailingSlashes(newPath);
                    break;
                case 1 << 1:
                    removeDuplicateSlashes(newPath);
                    break;
                case (1 << 0) | (1 << 1):
                default:
                    removeExcessiveSlashes(newPath);
                    break;
            }
            if (redirect_)
                callback(HttpResponse::newRedirectionResponse(newPath));
            else
            {
                req->setPath(newPath);
                app().forward(req, std::move(callback));
            }
        });
}

void SlashRemover::shutdown()
{
    LOG_TRACE << "SlashRemover plugin is shutdown!";
}
