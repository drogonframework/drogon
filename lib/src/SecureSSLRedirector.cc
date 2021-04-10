/**
 *
 *  drogon_plugin_SecureSSLRedirector.cc
 *
 */
#include <drogon/drogon.h>
#include <drogon/plugins/SecureSSLRedirector.h>
#include <string>

using namespace drogon;
using namespace drogon::plugin;

void SecureSSLRedirector::initAndStart(const Json::Value &config)
{
    if (config.isMember("ssl_redirect_exempt") &&
        config["ssl_redirect_exempt"].isArray())
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
    secureHost_ = config.get("secure_ssl_host", "").asString();
    app().registerSyncAdvice([this](const HttpRequestPtr &req) {
        return this->redirectingAdvice(req);
    });
}

void SecureSSLRedirector::shutdown()
{
    /// Shutdown the plugin
}

HttpResponsePtr SecureSSLRedirector::redirectingAdvice(
    const HttpRequestPtr &req) const
{
    if (req->isOnSecureConnection())
    {
        return HttpResponsePtr{};
    }
    else if (regexFlag_)
    {
        std::smatch regexResult;
        if (std::regex_match(req->path(), regexResult, exemptPegex_))
        {
            return HttpResponsePtr{};
        }
        else
        {
            return redirectToSSL(req);
        }
    }
    else
    {
        return redirectToSSL(req);
    }
}

HttpResponsePtr SecureSSLRedirector::redirectToSSL(
    const HttpRequestPtr &req) const
{
    if (!secureHost_.empty())
    {
        static std::string urlPrefix{"https://" + secureHost_};
        std::string query{urlPrefix + req->path()};
        if (!req->query().empty())
        {
            query += "?" + req->query();
        }
        return HttpResponse::newRedirectionResponse(query);
    }
    else
    {
        const auto &host = req->getHeader("host");
        if (!host.empty())
        {
            std::string query{"https://" + host};
            query += req->path();
            if (!req->query().empty())
            {
                query += "?" + req->query();
            }
            return HttpResponse::newRedirectionResponse(query);
        }
        else
        {
            return HttpResponse::newNotFoundResponse();
        }
    }
}
