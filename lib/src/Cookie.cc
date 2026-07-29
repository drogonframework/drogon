/**
 *
 *  Cookie.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/Cookie.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
using namespace drogon;

std::string Cookie::cookieString() const
{
    // Strip CR and LF from attacker-influenceable fields so that a value
    // containing "\r\n" cannot inject additional headers / split the
    // response via the Set-Cookie line.
    auto sanitize = [](const std::string &in) {
        std::string out;
        out.reserve(in.size());
        for (char c : in)
            if (c != '\r' && c != '\n')
                out.push_back(c);
        return out;
    };
    const std::string key = sanitize(key_);
    const std::string value = sanitize(value_);

    constexpr std::string_view prefix = "Set-Cookie: ";
    std::string ret;
    // reserve space to reduce frequency allocation
    ret.reserve(prefix.size() + key.size() + value.size() + 30);
    ret = prefix;
    ret.append(key).append("=").append(value).append("; ");
    if (expiresDate_.microSecondsSinceEpoch() !=
            (std::numeric_limits<int64_t>::max)() &&
        expiresDate_.microSecondsSinceEpoch() >= 0)
    {
        ret.append("Expires=")
            .append(utils::getHttpFullDateStr(expiresDate_))
            .append("; ");
    }
    if (maxAge_.has_value())
    {
        ret.append("Max-Age=")
            .append(std::to_string(maxAge_.value()))
            .append("; ");
    }
    if (!domain_.empty())
    {
        ret.append("Domain=").append(sanitize(domain_)).append("; ");
    }
    if (!path_.empty())
    {
        ret.append("Path=").append(sanitize(path_)).append("; ");
    }
    if (sameSite_ != SameSite::kNull)
    {
        switch (sameSite_)
        {
            case SameSite::kLax:
                ret.append("SameSite=Lax; ");
                break;
            case SameSite::kStrict:
                ret.append("SameSite=Strict; ");
                break;
            case SameSite::kNone:
                ret.append("SameSite=None; ");
                // Cookies with SameSite=None must now also specify the Secure
                // attribute (they require a secure context/HTTPS).
                ret.append("Secure; ");
                break;
            default:
                // Lax replaced None as the default value to ensure that users
                // have reasonably robust defense against some CSRF attacks
                ret.append("SameSite=Lax; ");
        }
    }
    if ((secure_ && sameSite_ != SameSite::kNone) || partitioned_)
    {
        ret.append("Secure; ");
    }
    if (httpOnly_)
    {
        ret.append("HttpOnly; ");
    }
    if (partitioned_)
    {
        ret.append("Partitioned; ");
    }
    ret.resize(ret.length() - 2);  // delete last semicolon
    ret.append("\r\n");
    return ret;
}
