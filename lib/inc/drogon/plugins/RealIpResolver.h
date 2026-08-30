/**
 *
 *  RealIpResolver.h
 *
 */

#pragma once

#include <drogon/plugins/Plugin.h>
#include <trantor/net/InetAddress.h>
#include <drogon/HttpRequest.h>
#include <vector>

namespace drogon
{
namespace plugin
{
/**
* @brief This plugin is used to resolve client real ip from HTTP request.
* @note This plugin currently supports only ipv4 address or cidr.
*
* The json configuration is as follows:
*
* @code
  {
     "name": "drogon::plugin::RealIpResolver",
     "dependencies": [],
     "config": {
        // Trusted proxy ip or cidr
        "trust_ips": ["127.0.0.1", "172.16.0.0/12"],
        // Which header to parse ip form. Default is x-forwarded-for
        "from_header": "x-forwarded-for",
        // The result will be inserted to HttpRequest attribute map with this
        // key. Default is "real-ip"
        "attribute_key": "real-ip"
     }
  }
  @endcode
*
* Enable the plugin by adding the configuration to the list of plugins in the
* configuration file.
*
*/
class DROGON_EXPORT RealIpResolver : public drogon::Plugin<RealIpResolver>
{
  public:
    RealIpResolver()
    {
    }

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    static const trantor::InetAddress &GetRealAddr(
        const drogon::HttpRequestPtr &req);

  private:
    const trantor::InetAddress &getRealAddr(
        const drogon::HttpRequestPtr &req) const;

    struct CIDR
    {
        explicit CIDR(const std::string &ipOrCidr);
        in_addr_t addr_{0};
        in_addr_t mask_{32};
    };

    using CIDRs = std::vector<CIDR>;
    static bool matchCidr(const trantor::InetAddress &addr,
                          const CIDRs &trustCIDRs);

    friend class Hodor;
    CIDRs trustCIDRs_;
    std::string fromHeader_;
    std::string attributeKey_;
    bool useXForwardedFor_{false};
};
}  // namespace plugin
}  // namespace drogon
