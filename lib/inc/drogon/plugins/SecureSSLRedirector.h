/**
 *
 *  @file drogon_plugin_SecureSSLRedirector.h
 *
 */

#pragma once
#include <drogon/exports.h>
#include <drogon/drogon_callbacks.h>
#include <drogon/plugins/Plugin.h>
#include <regex>

namespace drogon
{
namespace plugin
{
/**
 * @brief This plugin is used to redirect all non-HTTPS requests to HTTPS
 * (except for those URLs matching a regular expression listed in
 * the 'ssl_redirect_exempt' list).
 *
 * The json configuration is as follows:
 *
 * @code
   {
      "name": "drogon::plugin::SecureSSLRedirector",
      "dependencies": [],
      "config": {
            "ssl_redirect_exempt": ["^/.*\\.jpg", ...],
            "secure_ssl_host": "localhost:8849"
      }
   }
   @endcode
 *
 * ssl_redirect_exempt: a regular expression (for matching the path of a
 * request) list for URLs that don't have to be redirected.
 * secure_ssl_host: If this string is not empty, all SSL redirects
 * will be directed to this host rather than the originally-requested host.
 *
 * Enable the plugin by adding the configuration to the list of plugins in the
 * configuration file.
 *
 */
class DROGON_EXPORT SecureSSLRedirector
    : public drogon::Plugin<SecureSSLRedirector>
{
  public:
    SecureSSLRedirector()
    {
    }
    /// This method must be called by drogon to initialize and start the plugin.
    /// It must be implemented by the user.
    virtual void initAndStart(const Json::Value &config) override;

    /// This method must be called by drogon to shutdown the plugin.
    /// It must be implemented by the user.
    virtual void shutdown() override;

  private:
    HttpResponsePtr redirectingAdvice(const HttpRequestPtr &) const;
    HttpResponsePtr redirectToSSL(const HttpRequestPtr &) const;

    std::regex exemptPegex_;
    bool regexFlag_{false};
    std::string secureHost_;
};

}  // namespace plugin
}  // namespace drogon
