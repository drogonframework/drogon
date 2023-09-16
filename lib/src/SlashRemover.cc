#include <drogon/plugins/SlashRemover.h>
#include <drogon/plugins/Redirector.h>
#include <drogon/HttpAppFramework.h>
#include "drogon/utils/FunctionTraits.h"
#include <functional>
#include <string>
#include <regex>

using namespace drogon;
using namespace drogon::plugin;
using std::string;

#define TRAILING_SLASH_REGEX ".+\\/$"
#define DUPLICATE_SLASH_REGEX ".*\\/{2,}.*"

enum removeSlashMode : uint8_t
{
    trailing = 1 << 0,
    duplicate = 1 << 1,
    both = trailing | duplicate,
};

inline constexpr const char* regexes[] = {
    TRAILING_SLASH_REGEX,
    DUPLICATE_SLASH_REGEX,
    TRAILING_SLASH_REGEX "|" DUPLICATE_SLASH_REGEX,
};

static inline bool removeTrailingSlashes(string& url)
{
    const auto notSlashIndex = url.find_last_not_of('/');
    if (notSlashIndex == string::npos)  // Root
    {
        url.resize(1);
        return true;
    }
    url.resize(notSlashIndex + 1);
    return false;
}

static inline void removeDuplicateSlashes(string& url)
{
    size_t a = 1, len = url.size();
    for (; a < len && (url[a - 1] != '/' || url[a] != '/'); ++a)
        ;
    for (size_t b = a--; b < len; ++b)
    {
        const char c = url[b];
        if (c != '/' || url[a] != '/')
        {
            ++a;
            url[a] = c;
        }
    }
    url.resize(a + 1);
}

static inline void removeExcessiveSlashes(string& url)
{
    if (url.back() == '/' &&  // This check is so we don't search if there is no
                              // trailing slash to begin with
        removeTrailingSlashes(
            url))  // If it is root path, we don't need to check for duplicates
        return;

    removeDuplicateSlashes(url);
}

static inline bool handleReq(const drogon::HttpRequestPtr& req, int removeMode)
{
    static const std::regex regex(regexes[removeMode - 1]);
    if (std::regex_match(req->path(), regex))
    {
        string newPath = req->path();
        switch (removeMode)
        {
            case trailing:
                removeTrailingSlashes(newPath);
                break;
            case duplicate:
                removeDuplicateSlashes(newPath);
                break;
            case both:
            default:
                removeExcessiveSlashes(newPath);
                break;
        }
        req->setPath(std::move(newPath));
        return true;
    }
    return false;
}

void SlashRemover::initAndStart(const Json::Value& config)
{
    trailingSlashes_ = config.get("remove_trailing_slashes", true).asBool();
    duplicateSlashes_ = config.get("remove_duplicate_slashes", true).asBool();
    redirect_ = config.get("redirect", true).asBool();
    const uint8_t removeMode =
        (trailingSlashes_ * trailing) | (duplicateSlashes_ * duplicate);
    if (!removeMode)
        return;
    auto redirector = app().getPlugin<Redirector>();
    if (!redirector)
    {
        LOG_ERROR << "Redirector plugin is not found!";
        return;
    }
    auto func = [removeMode](const HttpRequestPtr& req) -> bool {
        return handleReq(req, removeMode);
    };
    if (redirect_)
    {
        redirector->registerPathRewriteHandler(std::move(func));
    }
    else
    {
        redirector->registerForwardHandler(std::move(func));
    }
}

void SlashRemover::shutdown()
{
    LOG_TRACE << "SlashRemover plugin is shutdown!";
}
