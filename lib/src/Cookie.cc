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
    ret.append(_key).append("= ").append(_value).append("; ");
    if (_expiresDate.microSecondsSinceEpoch() > 0)
    {
        ret.append("Expires= ").append(utils::getHttpFullDate(_expiresDate)).append("; ");
    }
    if (!_domain.empty())
    {
        ret.append("Domain= ").append(_domain).append("; ");
    }
    if (!_path.empty())
    {
        ret.append("Path= ").append(_path).append("; ");
    }
    if (_secure)
    {
        ret.append("Secure; ");
    }
    if (_httpOnly)
    {
        ret.append("HttpOnly; ");
    }
    ret.resize(ret.length() - 2); //delete last semicolon
    ret.append("\r\n");
    return ret;
}
