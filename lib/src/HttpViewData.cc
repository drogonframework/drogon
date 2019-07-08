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
    return htmlTranslate(string_view(str));
}

std::string HttpViewData::htmlTranslate(const string_view &str)
{
    std::string ret;
    ret.reserve(str.length() + 64);
    for (auto &ch : str)
    {
        switch (ch)
        {
            case '"':
                ret.append("&quot;", 6);
                break;
            case '&':
                ret.append("&amp;", 5);
                break;
            case '<':
                ret.append("&lt;", 4);
                break;
            case '>':
                ret.append("&gt;", 4);
                break;
            default:
                ret.push_back(ch);
                break;
        }
    }
    return ret;
}