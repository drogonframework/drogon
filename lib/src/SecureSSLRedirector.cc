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
    app().registerSyncAdvice([weakPtr](const HttpRequestPtr &req) {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
        {
            return HttpResponsePtr{};
        }
        return thisPtr->redirectingAdvice(req);
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
