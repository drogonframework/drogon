/**
 *
 *  @file RealIpResolver.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon. All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <drogon/plugins/RealIpResolver.h>
#include <cstdlib>
#include <cstring>

using namespace drogon;
using namespace drogon::plugin;

struct XForwardedForParser : public trantor::NonCopyable
{
    explicit XForwardedForParser(std::string value)
        : value_(std::move(value)), start_(value_.c_str()), len_(value_.size())
    {
    }

    std::string getNext()
    {
        if (len_ == 0)
        {
            return {};
        }
        // Skip trailing separators
        const char *cur;
        for (cur = start_ + len_ - 1; cur > start_; --cur, --len_)
        {
            if (*cur != ' ' && *cur != ',')
            {
                break;
            }
        }
        for (; cur > start_; --cur)
        {
            if (*cur == ' ' || *cur == ',')
            {
                ++cur;
                break;
            }
        }
        std::string ip{cur, len_ - (cur - start_)};
        len_ = cur == start_ ? 0 : cur - start_ - 1;
        return ip;
    }

  private:
    std::string value_;
    const char *start_;
    size_t len_;
};

/**
 * @brief Parse one entry of an address header into an InetAddress.
 *
 * Handles the shapes that occur in practice:
 *   1.2.3.4              ipv4, no port
 *   1.2.3.4:5678         ipv4 with port
 *   [2001:db8::1]:5678   bracketed ipv6 with port
 *   [2001:db8::1]        bracketed ipv6, no port
 *   2001:db8::1          bare ipv6, no port
 *
 * An unparseable token yields an unspecified address, which callers already
 * treat as "skip this entry".
 */
static trantor::InetAddress parseAddress(const std::string &addr)
{
    std::string ip = addr;
    uint16_t port = 0;

    if (ip.size() >= 2 && ip.front() == '[')
    {
        // Bracketed form - the brackets delimit the address, so anything after
        // "]:" is a port and the address itself may contain any number of
        // colons.
        auto close = ip.find(']');
        if (close == std::string::npos)
        {
            return trantor::InetAddress(ip, 0, true);
        }
        std::string host = ip.substr(1, close - 1);
        if (close + 1 < ip.size() && ip[close + 1] == ':')
        {
            port = static_cast<uint16_t>(
                std::strtoul(ip.c_str() + close + 2, nullptr, 10));
        }
        return trantor::InetAddress(host, port, true);
    }

    auto firstColon = ip.find(':');
    if (firstColon != std::string::npos && firstColon == ip.rfind(':'))
    {
        // Exactly one colon - this can only be "ipv4:port", because a valid
        // ipv6 address always contains at least two colons.
        port = static_cast<uint16_t>(
            std::strtoul(ip.c_str() + firstColon + 1, nullptr, 10));
        ip.resize(firstColon);
    }
    else if (firstColon != std::string::npos)
    {
        // Several colons and no brackets - a bare ipv6 address. It has no port
        // part, so the whole string is the address.
        return trantor::InetAddress(ip, 0, true);
    }

    // "::" is the only valid ipv6 address containing a single colon run, and it
    // is two colons; a lone ":" is not a valid separator pair. Try v4 first so
    // that malformed input keeps reporting unspecified, then fall back to v6.
    trantor::InetAddress v4(ip, port);
    if (!v4.isUnspecified())
    {
        return v4;
    }
    return trantor::InetAddress(ip, port, true);
}

void RealIpResolver::initAndStart(const Json::Value &config)
{
    fromHeader_ = config.get("from_header", "x-forwarded-for").asString();
    attributeKey_ = config.get("attribute_key", "real-ip").asString();

    std::transform(fromHeader_.begin(),
                   fromHeader_.end(),
                   fromHeader_.begin(),
                   [](unsigned char c) { return tolower(c); });
    if (fromHeader_ == "x-forwarded-for")
    {
        useXForwardedFor_ = true;
    }

    const Json::Value &trustIps = config["trust_ips"];
    if (!trustIps.isNull() && !trustIps.isArray())
    {
        throw std::runtime_error("Invalid trusted_ips. Should be array.");
    }
    for (const auto &ipOrCidr : trustIps)
    {
        trustCIDRs_.emplace_back(ipOrCidr.asString());
    }

    drogon::app().registerPreRoutingAdvice([this](const HttpRequestPtr &req) {
        const auto &headers = req->headers();
        auto ipHeaderFind = headers.find(fromHeader_);
        const trantor::InetAddress &peerAddr = req->getPeerAddr();
        if (ipHeaderFind == headers.end() || !matchCidr(peerAddr, trustCIDRs_))
        {
            // Target header is empty, or
            // direct peer is already a non-proxy
            req->attributes()->insert(attributeKey_, peerAddr);
            return;
        }
        const std::string &ipHeader = ipHeaderFind->second;
        // Use a header field which contains a single ip
        if (!useXForwardedFor_)
        {
            trantor::InetAddress addr = parseAddress(ipHeader);
            if (addr.isUnspecified())
            {
                req->attributes()->insert(attributeKey_, peerAddr);
            }
            else
            {
                req->attributes()->insert(attributeKey_, addr);
            }
            return;
        }
        // Use x-forwarded-for header, which may contains multiple ip address,
        // separated by comma
        XForwardedForParser parser(ipHeader);
        std::string ip;
        while (!(ip = parser.getNext()).empty())
        {
            trantor::InetAddress addr = parseAddress(ip);
            if (addr.isUnspecified() || matchCidr(addr, trustCIDRs_))
            {
                continue;
            }
            req->attributes()->insert(attributeKey_, addr);
            return;
        }
        // No match, use peerAddr
        req->attributes()->insert(attributeKey_, peerAddr);
    });
}

void RealIpResolver::shutdown()
{
}

const trantor::InetAddress &RealIpResolver::GetRealAddr(
    const HttpRequestPtr &req)
{
    auto *plugin = app().getPlugin<drogon::plugin::RealIpResolver>();
    if (!plugin)
    {
        return req->getPeerAddr();
    }
    return plugin->getRealAddr(req);
}

const trantor::InetAddress &RealIpResolver::getRealAddr(
    const HttpRequestPtr &req) const
{
    const std::shared_ptr<Attributes> &attributesPtr = req->getAttributes();
    if (!attributesPtr->find(attributeKey_))
    {
        return req->getPeerAddr();
    }
    return attributesPtr->get<trantor::InetAddress>(attributeKey_);
}

/**
 * @brief Check whether the first prefixLen_ bits of a network-order address
 *        match the first prefixLen_ bits of the network address.
 */
static bool comparePrefix(const std::string &addr, const std::string &network,
                          uint16_t prefixLen)
{
    if (addr.size() != network.size())
    {
        // Different address families can never match.
        return false;
    }
    const auto fullBytes = static_cast<size_t>(prefixLen / 8);
    const auto remainingBits = static_cast<uint8_t>(prefixLen % 8);
    if (fullBytes > 0 && std::memcmp(addr.data(), network.data(), fullBytes) != 0)
    {
        return false;
    }
    if (remainingBits == 0)
    {
        return true;
    }
    const auto mask = static_cast<uint8_t>(0xff << (8 - remainingBits));
    return (static_cast<uint8_t>(addr[fullBytes]) & mask) ==
           (static_cast<uint8_t>(network[fullBytes]) & mask);
}

bool RealIpResolver::matchCidr(const trantor::InetAddress &addr,
                               const CIDRs &trustCIDRs)
{
    const std::string ip = addr.toIpNetEndian();
    if (ip.empty())
    {
        return false;
    }
    for (const auto &cidr : trustCIDRs)
    {
        if (comparePrefix(ip, cidr.network_, cidr.prefixLen_))
        {
            return true;
        }
    }
    return false;
}

RealIpResolver::CIDR::CIDR(const std::string &ipOrCidr)
{
    // Find CIDR slash
    auto pos = ipOrCidr.find('/');
    std::string ip = ipOrCidr;
    if (pos != std::string::npos)
    {
        // parameter is a CIDR block
        ip = ipOrCidr.substr(0, pos);
        prefixLen_ = static_cast<uint16_t>(std::stoi(ipOrCidr.substr(pos + 1)));
    }

    trantor::InetAddress addr(ip, 0);
    if (addr.isUnspecified())
    {
        // The string constructor defaults to ipv4, so retry as ipv6 before
        // rejecting the entry.
        addr = trantor::InetAddress(ip, 0, true);
        if (addr.isUnspecified())
        {
            throw std::runtime_error("Bad ip address: " + ip);
        }
    }

    if (pos == std::string::npos)
    {
        prefixLen_ = addr.isIpV6() ? 128 : 32;
    }
    else if (prefixLen_ > (addr.isIpV6() ? 128 : 32))
    {
        throw std::runtime_error("Bad CIDR block: " + ipOrCidr);
    }

    network_ = addr.toIpNetEndian();
}
