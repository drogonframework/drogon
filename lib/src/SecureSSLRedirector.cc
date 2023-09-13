/**
 *
 *  drogon_plugin_SecureSSLRedirector.cc
 *
 */
#include <drogon/drogon.h>
#include <drogon/plugins/SecureSSLRedirector.h>
#include <drogon/plugins/Redirector.h>
#include <string>

using namespace drogon;
using namespace drogon::plugin;

void SecureSSLRedirector::initAndStart(const Json::Value &config)
{
    if (config.isMember("ssl_redirect_exempt"))
    {
        if (config["ssl_redirect_exempt"].isArray())
        {
            std::string regexString;
            for (auto &exempt : config["ssl_redirect_exempt"])
            {
                assert(exempt.isString());
                regexString.append("(").append(exempt.asString()).append(")|");
            }
            if (!regexString.empty())
            {
                regexString.resize(regexString.length() - 1);
                exemptPegex_ = std::regex(regexString);
                regexFlag_ = true;
            }
        }
        else if (config["ssl_redirect_exempt"].isString())
        {
            exemptPegex_ = std::regex(config["ssl_redirect_exempt"].asString());
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
    redirector->registerHandler(
        [weakPtr](const drogon::HttpRequestPtr &req, std::string &location) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                return;
            }
            thisPtr->redirectingAdvice(req, location);
        });
}

void SecureSSLRedirector::shutdown()
{
    /// Shutdown the plugin
}

void SecureSSLRedirector::redirectingAdvice(const HttpRequestPtr &req,
                                            std::string &location) const
{
    if (req->isOnSecureConnection())
    {
        return;
    }
    else if (regexFlag_)
    {
        std::smatch regexResult;
        if (std::regex_match(req->path(), regexResult, exemptPegex_))
        {
            return;
        }
        else
        {
            redirectToSSL(req, location);
            return;
        }
    }
    else
    {
        redirectToSSL(req, location);
        return;
    }
}

void SecureSSLRedirector::redirectToSSL(const HttpRequestPtr &req,
                                        std::string &location) const
{
    if (!secureHost_.empty())
    {
        static std::string urlPrefix{"https://" + secureHost_};
        location = urlPrefix + req->path();
        if (!req->query().empty())
        {
            location += "?" + req->query();
        }
        return;
    }
    else
    {
        const auto &host = req->getHeader("host");
        if (!host.empty())
        {
            location = "https://" + host;
            location += req->path();
            if (!req->query().empty())
            {
                location += "?" + req->query();
            }
            return;
        }
        else
        {
            return;
        }
    }
}
