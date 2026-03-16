/**
 *
 *  @file ParsingUtils.h
 *  Shared parsing utilities for HTTP and multipart parsing
 *
 *  Copyright 2024, Drogon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <string_view>
#include <utility>
#include <cctype>

namespace drogon::utils {

/**
 * @brief Check if a string_view starts with another string_view
 */
inline bool startsWith(const std::string_view &a, const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if a string_view starts with another string_view (case-insensitive)
 */
inline bool startsWithIgnoreCase(const std::string_view &a,
                                  const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (::tolower(a[i]) != ::tolower(b[i]))
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Parse a single HTTP header line into name and value
 * @param begin Pointer to the start of the line
 * @param end Pointer to the end of the line (not including CRLF)
 * @return A pair of (header_name, header_value) string_views
 */
inline std::pair<std::string_view, std::string_view> parseLine(
    const char *begin,
    const char *end)
{
    auto p = begin;
    while (p != end)
    {
        if (*p == ':')
        {
            if (p + 1 != end && *(p + 1) == ' ')
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 2, end - p - 2));
            }
            else
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 1, end - p - 1));
            }
        }
        ++p;
    }
    return std::make_pair(std::string_view(), std::string_view());
}

}  // namespace drogon::utils
