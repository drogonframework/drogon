#include <drogon/plugins/HostRedirector.h>
#include <drogon/plugins/Redirector.h>
#include <drogon/HttpAppFramework.h>
#include "drogon/utils/FunctionTraits.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>

using namespace drogon;
using namespace drogon::plugin;
using std::string;
using std::string_view;

bool HostRedirector::redirectingAdvice(const HttpRequestPtr& req,
                                       string& host,
                                       bool& pathChanged)
{
    const string& reqHost = host.empty() ? req->getHeader("host") : host;
    auto findRule = rules_.find(reqHost);
    if (findRule != rules_.end())
    {
        auto& rule = findRule->second;
        auto& paths = rule.paths;
        auto& path = req->path();
        auto findPath = paths.find(req->path());
        if (findPath != paths.end())
        {
            auto& redirectToHost = rule.redirectToHost;
            if (redirectToHost != reqHost)
                host = redirectToHost;

            auto& redirectToPath = rule.redirectToPath;
            if (redirectToPath != path)
            {
                req->setPath(redirectToPath);
                pathChanged = true;
            }
        }
    }
    return true;
}

void HostRedirector::initAndStart(const Json::Value& config)
{
    if (config.isMember("rules"))
    {
        const auto& rules = config["rules"];
        if (rules.isObject())
        {
            for (const auto& redirectTo : rules)
            {
                if (!redirectTo.isString())
                    continue;

                const string redirectToStr = redirectTo.asString();
                string redirectToHost, redirectToPath;
                auto pathIndex = redirectToStr.find_first_of('/');
                if (pathIndex != string::npos)
                {
                    redirectToHost = redirectToStr.substr(0, pathIndex);
                    redirectToPath = redirectToStr.substr(pathIndex);
                }
                else
                    redirectToPath = "/";

                for (const auto& redirectFrom : rules[redirectToStr])
                {
                    if (!redirectFrom.isString())
                        continue;

                    const string redirectFromStr = redirectFrom.asString();
                    string redirectFromHost, redirectFromPath;
                    pathIndex = redirectFromStr.find_first_of('/');
                    if (pathIndex != string::npos)
                    {
                        redirectFromHost = redirectFromStr.substr(0, pathIndex);
                        redirectFromPath = redirectFromStr.substr(pathIndex);
                    }
                    else
                        redirectFromPath = "/";

                    auto& rule =
                        rules_[redirectFromHost.empty() ? redirectFromStr
                                                        : redirectFromHost];
                    rule.redirectToHost =
                        redirectToHost.empty() ? redirectToStr : redirectToHost;
                    rule.redirectToPath = redirectToPath;
                    rule.paths.insert(redirectFromPath);
                }
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
        [weakPtr](const HttpRequestPtr& req,
                  string& host,
                  bool& pathChanged) -> bool {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                return false;
            }
            return thisPtr->redirectingAdvice(req, host, pathChanged);
        });
}

void HostRedirector::shutdown()
{
    LOG_TRACE << "HostRedirector plugin is shutdown!";
}
