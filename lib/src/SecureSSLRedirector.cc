/**
 *
 *  drogon_plugin_SecureSSLRedirector.cc
 *
 */
#include <drogon/drogon.h>
#include <drogon/plugins/SecureSSLRedirector.h>
#include <drogon/plugins/Redirector.h>
#include <cstddef>
#include <string>

using namespace drogon;
using namespace drogon::plugin;

void SecureSSLRedirector::initAndStart(const Json::Value &config)
{
    if (config.isMember("ssl_redirect_exempt"))
    {
        if (config["ssl_redirect_exempt"].isArray())
        {
            const auto &exempts = config["ssl_redirect_exempt"];
            size_t exemptsCount = exempts.size();
            if (exemptsCount)
            {
                std::string regexString;
                size_t len = 0;
                for (const auto &exempt : exempts)
                {
                    assert(exempt.isString());
                    len += exempt.size();
                }
                regexString.reserve((exemptsCount * (1 + 2)) - 1 + len);

                const auto last = --exempts.end();
                for (auto exempt = exempts.begin(); exempt != last; ++exempt)
                    regexString.append("(")
                        .append(exempt->asString())
                        .append(")|");
                regexString.append("(").append(last->asString()).append(")");

                exemptRegex_ = std::regex(regexString);
                regexFlag_ = true;
            }
        }
        else if (config["ssl_redirect_exempt"].isString())
        {
            exemptRegex_ = std::regex(config["ssl_redirect_exempt"].asString());
            regexFlag_ = true;
        }
        else
        {
            LOG_ERROR
                << "ssl_redirect_exempt must be a string or string array!";
        }
    }
    secureHost_ = config.get("secure_ssl_host", "").asString();
    std::weak_ptr<SecureSSLRedirector> weakPtr = shared_from_this();
    auto redirector = drogon::app().getPlugin<Redirector>();
    if (!redirector)
    {
        LOG_ERROR << "Redirector plugin is not found!";
        return;
    }
    redirector->registerRedirectHandler(
        [weakPtr](const drogon::HttpRequestPtr &req,
                  std::string &protocol,
                  std::string &host,
                  bool &) -> bool {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                return false;
            }
            return thisPtr->redirectingAdvice(req, protocol, host);
        });
}

void SecureSSLRedirector::shutdown()
{
    /// Shutdown the plugin
}

bool SecureSSLRedirector::redirectingAdvice(const HttpRequestPtr &req,
                                            std::string &protocol,
                                            std::string &host) const
{
    if (req->isOnSecureConnection() || protocol == "https://")
    {
        return true;
    }
    else if (regexFlag_)
    {
        std::smatch regexResult;
        if (std::regex_match(req->path(), regexResult, exemptRegex_))
        {
            return true;
        }
        else
        {
            return redirectToSSL(req, protocol, host);
        }
    }
    else
    {
        return redirectToSSL(req, protocol, host);
    }
}

bool SecureSSLRedirector::redirectToSSL(const HttpRequestPtr &req,
                                        std::string &protocol,
                                        std::string &host) const
{
    if (!secureHost_.empty())
    {
        host = secureHost_;
        protocol = "https://";
        return true;
    }
    else if (host.empty())
    {
        const auto &reqHost = req->getHeader("host");
        if (!reqHost.empty())
        {
            protocol = "https://";
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        protocol = "https://";
        return true;
    }
}
