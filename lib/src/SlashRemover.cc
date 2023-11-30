#include <drogon/plugins/SlashRemover.h>
#include <drogon/plugins/Redirector.h>
#include <drogon/HttpAppFramework.h>
#include "drogon/utils/FunctionTraits.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

using namespace drogon;
using namespace drogon::plugin;
using std::string;
using std::string_view;

enum removeSlashMode : uint8_t
{
    trailing = 1 << 0,
    duplicate = 1 << 1,
    both = trailing | duplicate,
};

/// Returns the index before the trailing slashes,
/// or 0 if only contains slashes
static inline size_t findTrailingSlashes(string_view url)
{
    auto len = url.size();
    // Must be at least 2 chars and end with a slash
    if (len < 2 || url.back() != '/')
        return string::npos;

    size_t a = len - 1;  // We already know the last char is '/',
                         // we will use pre-decrement to account for this
    while (--a > 0 && url[a] == '/')
        ;  // We know the first char is '/', so don't check for 0
    return a;
}

static inline void removeTrailingSlashes(string &url,
                                         size_t start,
                                         string_view originalUrl)
{
    url = originalUrl.substr(0, start + 1);
}

/// Returns the index of the 2nd duplicate slash
static inline size_t findDuplicateSlashes(string_view url)
{
    size_t len = url.size();
    if (len < 2)
        return string::npos;

    bool startedPair = true;  // Always starts with a slash
    for (size_t a = 1; a < len; ++a)
    {
        if (url[a] != '/')  // Broken pair
        {
            startedPair = false;
            continue;
        }
        if (startedPair)  // Matching pair
            return a;
        startedPair = true;
    }

    return string::npos;
}

static inline void removeDuplicateSlashes(string &url, size_t start)
{
    // +1 because we don't need to look at the same character again,
    // which was found by `findDuplicateSlashes`, it saves one iteration
    for (size_t b = (start--) + 1, len = url.size(); b < len; ++b)
    {
        const char c = url[b];
        if (c != '/' || url[start] != '/')
        {
            ++start;
            url[start] = c;
        }
    }
    url.resize(start + 1);
}

static inline std::pair<size_t, size_t> findExcessiveSlashes(string_view url)
{
    size_t len = url.size();
    if (len < 2)  // Must have at least 2 characters to count as either trailing
                  // or duplicate slash
        return {string::npos, string::npos};

    // Trail finder
    size_t trailIdx = len;  // The pre-decrement will put it on last char
    while (--trailIdx > 0 && url[trailIdx] == '/')
        ;  // We know first char is '/', no need to check it

    // Filled with '/'
    if (trailIdx == 0)
        return {
            0,             // Only keep first slash
            string::npos,  // No duplicate
        };

    // Look for a duplicate pair
    size_t dupIdx = 1;
    for (bool startedPair = true; dupIdx < trailIdx;
         ++dupIdx)  // Always starts with a slash
    {
        if (url[dupIdx] != '/')  // Broken pair
        {
            startedPair = false;
            continue;
        }
        if (startedPair)  // Matching pair
            break;
        startedPair = true;
    }

    // Found no duplicate
    if (dupIdx == trailIdx)
        return {
            trailIdx != len - 1
                ?  // If has gone past last char, then there is a trailing slash
                trailIdx
                : string::npos,  // No trail
            string::npos,        // No duplicate
        };

    // Duplicate found
    return {
        trailIdx != len - 1
            ?  // If has gone past last char, then there is a trailing slash
            trailIdx
            : string::npos,  // No trail
        dupIdx,
    };
}

static inline void removeExcessiveSlashes(string &url,
                                          std::pair<size_t, size_t> start,
                                          string_view originalUrl)
{
    if (start.first != string::npos)
        removeTrailingSlashes(url, start.first, originalUrl);
    else
        url = originalUrl;

    if (start.second != string::npos)
        removeDuplicateSlashes(url, start.second);
}

static inline bool handleReq(const drogon::HttpRequestPtr &req,
                             uint8_t removeMode)
{
    switch (removeMode)
    {
        case trailing:
        {
            auto find = findTrailingSlashes(req->path());
            if (find == string::npos)
                return false;

            string newPath;
            removeTrailingSlashes(newPath, find, req->path());
            req->setPath(std::move(newPath));
            break;
        }
        case duplicate:
        {
            auto find = findDuplicateSlashes(req->path());
            if (find == string::npos)
                return false;

            string newPath = req->path();
            removeDuplicateSlashes(newPath, find);
            req->setPath(std::move(newPath));
            break;
        }
        case both:
        default:
        {
            auto find = findExcessiveSlashes(req->path());
            if (find.first == string::npos && find.second == string::npos)
                return false;

            string newPath;
            removeExcessiveSlashes(newPath, find, req->path());
            req->setPath(std::move(newPath));
            break;
        }
    }
    return true;
}

void SlashRemover::initAndStart(const Json::Value &config)
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
    auto func = [removeMode](const HttpRequestPtr &req) -> bool {
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
