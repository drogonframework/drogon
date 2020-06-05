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
    ret.append(key_).append("= ").append(value_).append("; ");
    if (expiresDate_.microSecondsSinceEpoch() !=
            (std::numeric_limits<int64_t>::max)() &&
        expiresDate_.microSecondsSinceEpoch() >= 0)
    {
        ret.append("Expires= ")
            .append(utils::getHttpFullDate(expiresDate_))
            .append("; ");
    }
    if (!domain_.empty())
    {
        ret.append("Domain= ").append(domain_).append("; ");
    }
    if (!path_.empty())
    {
        ret.append("Path= ").append(path_).append("; ");
    }
    if (secure_)
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
