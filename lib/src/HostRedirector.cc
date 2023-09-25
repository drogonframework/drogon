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
    auto find = rules_.find(reqHost);
    if (find != rules_.end())
        host = find->second;

    // TODO: some may need to change the path as well

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
                for (const auto& redirectFrom : rules[redirectToStr])
                {
                    if (!redirectFrom.isString())
                        continue;

                    rules_[redirectFrom.asString()] = redirectToStr;
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
    redirector->registerRedirectHandler([weakPtr](const HttpRequestPtr& req,
                                                  string&,
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
