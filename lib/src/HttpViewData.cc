/**
 *
 *  HttpViewData.cc
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

#include <drogon/HttpViewData.h>

using namespace drogon;

std::string HttpViewData::htmlTranslate(const std::string &str)
{
    std::string ret;
    ret.reserve(str.length());
    for (auto &ch : str)
    {
        switch (ch)
        {
        case '"':
            ret.append("&quot;");
            break;
        case '<':
            ret.append("&lt;");
            break;
        case '>':
            ret.append("&gt;");
            break;
        case '&':
            ret.append("&amp;");
            break;
        default:
            ret.push_back(ch);
            break;
        }
    }
    return ret;
}