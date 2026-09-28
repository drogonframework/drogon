/**
 *
 *  RealIpResolver.h
 *
 */

#pragma once

#include <drogon/plugins/Plugin.h>
#include <trantor/net/InetAddress.h>
#include <drogon/HttpRequest.h>
#include <cstdint>
#include <string>
#include <vector>

namespace drogon
{
namespace plugin
{
/**
* @brief This plugin is used to resolve client real ip from HTTP request.
* @note This plugin supports both ipv4 and ipv6 address or cidr.
*
* The json configuration is as follows:
*
* @code
  {
     "name": "drogon::plugin::RealIpResolver",
     "dependencies": [],
     "config": {
        // Trusted proxy ip or cidr. Both ipv4 and ipv6 are accepted.
        "trust_ips": ["127.0.0.1", "172.16.0.0/12", "::1", "2001:db8::/32"],
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
        /**
         * @brief The network address in network byte order.
         *
         * 4 bytes for an ipv4 CIDR, 16 bytes for an ipv6 one. The length also
         * identifies the address family, so an ipv4 address can never match an
         * ipv6 CIDR (and vice versa) - comparing byte strings of different
         * lengths fails early.
         */
        std::string network_;
        /**
         * @brief Number of significant leading bits of network_: 0-32 for ipv4
         * and 0-128 for ipv6.
         */
        uint16_t prefixLen_{32};
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
