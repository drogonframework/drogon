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
    if (!redirector)
    {
        LOG_ERROR << "Redirector plugin is not found!";
        return;
    }
    redirector->registerHandler([weakPtr](const drogon::HttpRequestPtr &req,
                                          std::string &protocol,
                                          std::string &host,
                                          std::string &path) -> bool {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
        {
            return false;
        }
        return thisPtr->redirectingAdvice(req, protocol, host, path);
    });
}

void SecureSSLRedirector::shutdown()
{
    /// Shutdown the plugin
}

bool SecureSSLRedirector::redirectingAdvice(const HttpRequestPtr &req,
                                            std::string &protocol,
                                            std::string &host,
                                            bool &pathChanged) const
{
    if (req->isOnSecureConnection() || protocol == "https://")
    {
        return true;
    }
    else if (regexFlag_)
    {
        std::smatch regexResult;
        if (std::regex_match(req->path(), regexResult, exemptPegex_))
        {
            return true;
        }
        else
        {
            return redirectToSSL(req, protocol, host, pathChanged);
        }
    }
    else
    {
        return redirectToSSL(req, protocol, host, pathChanged);
    }
}

bool SecureSSLRedirector::redirectToSSL(const HttpRequestPtr &req,
                                        std::string &protocol,
                                        std::string &host,
                                        bool &pathChanged) const
{
    (void)pathChanged;
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
