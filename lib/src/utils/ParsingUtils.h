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

#include <cctype>
#include <string_view>
#include <utility>

namespace drogon
{
namespace utils
{

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
 * @brief Check if a string_view starts with another string_view
 * (case-insensitive)
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
        const auto lhs = std::tolower(static_cast<unsigned char>(a[i]));
        const auto rhs = std::tolower(static_cast<unsigned char>(b[i]));
        if (lhs != rhs)
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

inline bool parseHeaderParameter(std::string_view headerValue,
                                 std::string_view parameterName,
                                 std::string_view &parameterValue)
{
    size_t pos = 0;
    while ((pos = headerValue.find(';', pos)) != std::string_view::npos)
    {
        ++pos;
        while (pos < headerValue.size() &&
               (headerValue[pos] == ' ' || headerValue[pos] == '\t'))
        {
            ++pos;
        }

        const auto nameBegin = pos;
        while (pos < headerValue.size() && headerValue[pos] != '=' &&
               headerValue[pos] != ';')
        {
            ++pos;
        }
        if (pos == headerValue.size() || headerValue[pos] != '=')
        {
            continue;
        }

        auto nameEnd = pos;
        while (nameEnd > nameBegin && (headerValue[nameEnd - 1] == ' ' ||
                                       headerValue[nameEnd - 1] == '\t'))
        {
            --nameEnd;
        }

        ++pos;
        while (pos < headerValue.size() &&
               (headerValue[pos] == ' ' || headerValue[pos] == '\t'))
        {
            ++pos;
        }

        auto valueBegin = pos;
        size_t valueEnd;
        if (pos < headerValue.size() && headerValue[pos] == '"')
        {
            valueBegin = ++pos;
            valueEnd = headerValue.find('"', pos);
            if (valueEnd == std::string_view::npos)
            {
                return false;
            }
            pos = valueEnd + 1;
        }
        else
        {
            valueEnd = headerValue.find(';', pos);
            if (valueEnd == std::string_view::npos)
            {
                valueEnd = headerValue.size();
            }
            pos = valueEnd;
            while (valueEnd > valueBegin && (headerValue[valueEnd - 1] == ' ' ||
                                             headerValue[valueEnd - 1] == '\t'))
            {
                --valueEnd;
            }
        }

        if (headerValue.substr(nameBegin, nameEnd - nameBegin) == parameterName)
        {
            parameterValue =
                headerValue.substr(valueBegin, valueEnd - valueBegin);
            return true;
        }
    }
    return false;
}

}  // namespace utils
}  // namespace drogon
