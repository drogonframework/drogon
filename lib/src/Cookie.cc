/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/Cookie.h>
#include <drogon/utils/Utilities.h>
using namespace drogon;
const std::string Cookie::cookieString() const
{
    std::string ret = "Set-Cookie: ";
    ret.append(_key).append("= ").append(_value).append("; ");
    if (_expiresDate.microSecondsSinceEpoch() > 0)
    {
        //add expiresDate string
        //        struct tm tm1=_expiresDate.tmStruct();
        //        char timeBuf[64];
        //        asctime_r(&tm1,timeBuf);
        //        std::string timeStr(timeBuf);
        //        std::string::size_type len=timeStr.length();
        //        timeStr =timeStr.substr(0,len-1)+" GMT";

        ret.append("Expires= ").append(getHttpFullDate(_expiresDate)).append("; ");
    }
    if (!_domain.empty())
    {
        ret.append("Domain= ").append(_domain).append("; ");
    }
    if (!_path.empty())
    {
        ret.append("Path= ").append(_path).append("; ");
    }
    if (_ensure)
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
