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
using namespace drogon;
std::string Cookie::cookieString() const
{
    std::string ret = "Set-Cookie: ";
    // reserve space to reduce frequency allocation
    ret.reserve(ret.size() + key_.size() + value_.size() + 30);
    ret.append(key_).append("=").append(value_).append("; ");
    if (expiresDate_.microSecondsSinceEpoch() !=
            (std::numeric_limits<int64_t>::max)() &&
        expiresDate_.microSecondsSinceEpoch() >= 0)
    {
        ret.append("Expires=")
            .append(utils::getHttpFullDate(expiresDate_))
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
        ret.append("Domain=").append(domain_).append("; ");
    }
    if (!path_.empty())
    {
        ret.append("Path=").append(path_).append("; ");
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
    if (secure_ && sameSite_ != SameSite::kNone)
    {
        ret.append("Secure; ");
    }
    if (httpOnly_)
    {
        ret.append("HttpOnly; ");
    }
    ret.resize(ret.length() - 2);  // delete last semicolon
    ret.append("\r\n");
    return ret;
}
