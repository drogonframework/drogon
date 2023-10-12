#include <drogon/plugins/HostRedirector.h>
#include <drogon/plugins/Redirector.h>
#include <drogon/HttpAppFramework.h>
#include "drogon/utils/FunctionTraits.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace drogon;
using namespace drogon::plugin;
using std::string;
using std::string_view;

bool HostRedirector::redirectingAdvice(const HttpRequestPtr &req,
                                       string &host,
                                       bool &pathChanged) const
{
    const string &reqHost = host.empty() ? req->getHeader("host") : host;
    const string &reqPath = req->path();
    string newHost, path = reqPath;

    // Lookup host-specific rules first if they exist
    if (doHostLookup_ && !reqHost.empty())
    {
        newHost = reqHost;
        lookup(newHost, path);
    }

    // Lookup within non-host rules
    {
        string temp;
        lookup(temp, path);
        if (!temp.empty())  // Altered
            newHost = std::move(temp);
    }

    if (!newHost.empty() && newHost != reqHost)
        host = std::move(newHost);

    if (path != reqPath)
    {
        req->setPath(std::move(path));
        pathChanged = true;
    }

    return true;  // This plugin only redirects and is not responsible for 404s
}

void HostRedirector::lookup(string &host, string &path) const
{
    do
    {
        auto findHost = rulesFrom_.find(host);
        if (findHost == rulesFrom_.end())
            break;

        if (path == "/")
        {
            const RedirectGroup &group = findHost->second;
            const RedirectLocation *location = group.to;
            if (location)  // Strict takes priority
            {
                host = location->host;
                path = location->path;
                continue;
            }
            location = group.wildcard;
            if (location)  // If strict didn't match, then it could match
                           // wildcard
            {
                host = location->host;
                path = location->path;
                continue;
            }
            break;  // Neither exist for root
        }

        bool isWildcard = true;
        const RedirectLocation *to = nullptr;
        size_t lastWildcardPathViewLen = 0;
        string_view pathView = path;
        for (const RedirectGroup *group = &(findHost->second);;)
        {
            const RedirectLocation *location = group->wildcard;
            bool pathIsExhausted = pathView.empty();
            if (location && (pathIsExhausted || pathView.front() == '/'))
            {
                to = location;
                lastWildcardPathViewLen = pathView.size();
            }

            const auto &groups = group->groups;
            auto maxPathLen = group->maxPathLen;
            if (!maxPathLen ||
                pathIsExhausted)  // Final node or path is shorter than tree
            {
                if (!pathIsExhausted)  // Path is longer than tree
                    break;

                location = group->to;
                if (location)
                {
                    to = location;
                    isWildcard = false;
                }
                break;
            }

            auto find = groups.find(pathView.substr(0, maxPathLen));
            if (find == groups.end())  // Cannot go deeper
                break;

            pathView = pathView.substr(maxPathLen);
            group = find->second;
        }

        if (to)
        {
            const string &toHost = to->host;
            bool hasToHost = !toHost.empty();
            if (hasToHost)
                host = toHost;

            if (isWildcard)
            {
                const string &toPath = to->path;
                string newPath;
                const auto len = path.size();
                auto start = len - lastWildcardPathViewLen;
                start += toPath.back() == '/' && path[start] == '/';
                newPath.reserve(toPath.size() + (len - start));

                newPath = toPath;
                newPath.append(path.substr(start));
                path = std::move(newPath);
            }
            else
                path = to->path;

            if (!doHostLookup_ &&
                hasToHost)  // If our maps don't contain hosts, no need to look
                            // it up next iteration
                break;
        }
        else
            break;
    } while (true);
}

void HostRedirector::initAndStart(const Json::Value &config)
{
    doHostLookup_ = false;
    if (config.isMember("rules"))
    {
        const auto &rules = config["rules"];
        if (rules.isObject())
        {
            const auto &redirectToList = rules.getMemberNames();
            rulesTo_.reserve(redirectToList.size());
            for (const string redirectToStr : redirectToList)
            {
                string redirectToHost, redirectToPath;
                auto pathIdx = redirectToStr.find('/');
                if (pathIdx != string::npos)
                {
                    redirectToHost = redirectToStr.substr(0, pathIdx);
                    redirectToPath = redirectToStr.substr(pathIdx);
                }
                else
                    redirectToPath = "/";

                const auto &redirectFromValue = rules[redirectToStr];

                auto toIdx = rulesTo_.size();
                rulesTo_.push_back({
                    std::move(redirectToHost.empty() && pathIdx != 0
                                  ? redirectToStr
                                  : redirectToHost),
                    std::move(redirectToPath),
                });

                if (redirectFromValue.isArray())
                {
                    for (const auto &redirectFrom : redirectFromValue)
                    {
                        assert(redirectFrom.isString());

                        string redirectFromStr = redirectFrom.asString();
                        auto len = redirectFromStr.size();
                        bool isWildcard = false;
                        if (len > 1 && redirectFromStr[len - 2] == '/' &&
                            redirectFromStr[len - 1] == '*')
                        {
                            redirectFromStr.resize(len - 2);
                            isWildcard = true;
                        }

                        string redirectFromHost, redirectFromPath;
                        pathIdx = redirectFromStr.find('/');
                        if (pathIdx != string::npos)
                        {
                            redirectFromHost =
                                redirectFromStr.substr(0, pathIdx);
                            redirectFromPath = redirectFromStr.substr(pathIdx);
                        }
                        else
                            redirectFromPath = '/';

                        const string &fromHost =
                            redirectFromHost.empty() && pathIdx != 0
                                ? redirectFromStr
                                : redirectFromHost;
                        if (!fromHost.empty())
                            doHostLookup_ =
                                true;  // We have hosts in lookup rules
                        rulesFromData_.push_back({
                            std::move(fromHost),
                            std::move(redirectFromPath),
                            isWildcard,
                            toIdx,
                        });
                    }
                }
                // This commented block can be used to support {from: to}
                // syntax, but the JSON library must support multi-key objects
                /*else if(redirectFromValue.isString())
                {

                }*/
            }

            struct RedirectDepthGroup
            {
                std::vector<const RedirectFrom *> fromData;
                size_t maxPathLen;
            };

            std::unordered_map<RedirectGroup *, RedirectDepthGroup> leafs,
                leafsBackbuffer;  // Keep track of most recent leaf nodes

            // Find minimum required path length for each host
            for (const auto &redirectFrom : rulesFromData_)
            {
                const auto &path = redirectFrom.path;
                auto &rule = rulesFrom_[redirectFrom.host];
                if (path == "/")  // Root rules are part of the host group
                    continue;

                auto len = path.size();
                if (len < rule.maxPathLen)
                    rule.maxPathLen = len;
            }

            // Create initial leaf nodes
            for (const auto &redirectFrom : rulesFromData_)
            {
                string_view path = redirectFrom.path;
                auto &rule = rulesFrom_[redirectFrom.host];
                if (path == "/")
                {
                    (redirectFrom.isWildcard ? rule.wildcard : rule.to) =
                        &rulesTo_[redirectFrom.toIdx];
                    continue;
                }

                size_t maxLen = rule.maxPathLen;
                string_view pathGroup = path.substr(0, maxLen);

                auto &groups = rule.groups;
                auto find = groups.find(pathGroup);
                RedirectGroup *group;
                if (find != groups.end())
                    group = find->second;
                else
                {
                    group = new RedirectGroup;
                    groups[pathGroup] = group;
                }

                if (path.size() == maxLen)  // Reached the end
                {
                    (redirectFrom.isWildcard ? group->wildcard : group->to) =
                        &rulesTo_[redirectFrom.toIdx];
                    group->maxPathLen = 0;
                    continue;  // No need to queue this node up, it reached the
                               // end
                }

                auto &leaf = leafs[group];
                leaf.fromData.push_back(&redirectFrom);
                leaf.maxPathLen = maxLen;
            }

            // Populate subsequent leaf nodes
            while (!leafs.empty())
            {
                for (auto &[group, depthGroup] : leafs)
                {
                    size_t minIdx = depthGroup.maxPathLen,
                           maxIdx = string::npos;
                    const auto &fromData = depthGroup.fromData;
                    for (const auto &redirectFrom : fromData)
                    {
                        auto len = redirectFrom->path.size();
                        if (len >= minIdx && len < maxIdx)
                            maxIdx = len;
                    }

                    size_t maxLen = maxIdx - minIdx;
                    group->maxPathLen = maxLen;
                    for (const auto &redirectFrom : fromData)
                    {
                        string_view path = redirectFrom->path;
                        string_view pathGroup = path.substr(minIdx, maxLen);

                        auto &groups = group->groups;
                        auto find = groups.find(pathGroup);
                        RedirectGroup *childGroup;
                        if (find != groups.end())
                            childGroup = find->second;
                        else
                        {
                            childGroup = new RedirectGroup;
                            groups[pathGroup] = childGroup;
                        }

                        if (path.size() == maxIdx)  // Reached the end
                        {
                            (redirectFrom->isWildcard ? childGroup->wildcard
                                                      : childGroup->to) =
                                &rulesTo_[redirectFrom->toIdx];
                            childGroup->maxPathLen = 0;
                            continue;  // No need to queue this node up, it
                                       // reached the end
                        }

                        auto &leaf = leafsBackbuffer[childGroup];
                        leaf.fromData.push_back(redirectFrom);
                        leaf.maxPathLen = maxIdx;
                    }
                }

                leafs = leafsBackbuffer;
                leafsBackbuffer.clear();
            }
        }
    }
    std::weak_ptr<HostRedirector> weakPtr = shared_from_this();
    auto redirector = app().getPlugin<Redirector>();
    if (!redirector)
    {
        LOG_ERROR << "Redirector plugin is not found!";
        return;
    }
    redirector->registerPostRedirectorHandler(
        [weakPtr](const HttpRequestPtr &req,
                  string &host,
                  bool &pathChanged) -> bool {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                return false;
            }
            return thisPtr->redirectingAdvice(req, host, pathChanged);
        });
}

void HostRedirector::recursiveDelete(const RedirectGroup *group)
{
    for (const auto &[_, child] : group->groups)
        recursiveDelete(child);
    delete group;
}

void HostRedirector::shutdown()
{
    // Free up manually allocated memory of nodes
    for (const auto &[_, rule] : rulesFrom_)
    {
        // The rule value itself doesn't need manual freeing,
        // so start at a depth level of 2
        for (const auto &[_, group] : rule.groups)
            recursiveDelete(group);
    }

    LOG_TRACE << "HostRedirector plugin is shutdown!";
}
