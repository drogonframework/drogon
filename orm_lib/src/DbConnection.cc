/**
 *
 *  @file DbConnection.cc
 *  @author Martin Chang
 *
 *  Copyright 2020, Martin Chang.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbConnection.h"

#include <regex>

using namespace drogon::orm;

std::map<std::string, std::string> DbConnection::parseConnString(
    const std::string& connInfo)
{
    const static std::regex re(
        R"((\w+) *= *('(?:[^\n]|\\[^\n])+'|(?:\S|\\\S)+))");
    std::smatch what;
    std::map<std::string, std::string> params;
    std::string str = connInfo;

    while (std::regex_search(str, what, re))
    {
        assert(what.size() == 3);
        std::string key = what[1];
        std::string rawValue = what[2];
        std::string value;
        bool quoted =
            !rawValue.empty() && rawValue[0] == '\'' && rawValue.back() == '\'';

        value.reserve(rawValue.size());
        for (size_t i = 0; i < rawValue.size(); i++)
        {
            if (quoted && (i == 0 || i == rawValue.size() - 1))
                continue;
            else if (rawValue[i] == '\\' && i != rawValue.size() - 1)
            {
                value.push_back(rawValue[i + 1]);
                i += 1;
                continue;
            }

            value.push_back(rawValue[i]);
        }
        params[key] = value;
        str = what.suffix().str();
    }
    return params;
}
