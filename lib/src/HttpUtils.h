/**
 *
 *  @file HttpUtils.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <trantor/utils/MsgBuffer.h>
#include <drogon/utils/Utilities.h>
#include <cctype>
#include <utility>
#include <drogon/HttpTypes.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace drogon
{

namespace internal
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

struct HeaderParameter
{
    std::string_view name;
    std::string_view value;
    bool hasValue{false};
};

// Return the value before semicolon-delimited parameters without parsing them.
// MIME lookups only need this prefix, so keep this operation as a single find.
inline std::string_view headerValueBeforeParameters(std::string_view value)
{
    return value.substr(0, value.find(';'));
}

// Consume one semicolon-delimited item. The first item can be a cookie pair,
// a media type, or a Set-Cookie name/value pair; later items are parameters.
// Quotes protect semicolons, and the returned views point into the input.
inline bool nextHeaderParameter(std::string_view &remaining,
                                HeaderParameter &parameter)
{
    if (remaining.empty())
        return false;
    size_t end = 0;
    bool quoted = false;
    for (; end < remaining.size(); ++end)
    {
        const char c = remaining[end];
        if (quoted && c == '\\' && end + 1 < remaining.size())
            ++end;
        else if (c == '"')
            quoted = !quoted;
        else if (c == ';' && !quoted)
            break;
    }
    if (quoted)
    {
        remaining = {};
        return false;
    }
    const auto item = utils::trim(remaining.substr(0, end));
    remaining = end == remaining.size() ? std::string_view{}
                                        : remaining.substr(end + 1);
    const auto equal = item.find('=');
    parameter.name = utils::trim(item.substr(0, equal));
    parameter.hasValue = equal != std::string_view::npos;
    parameter.value = parameter.hasValue ? utils::trim(item.substr(equal + 1))
                                         : std::string_view{};
    return true;
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

        const bool matchingName =
            headerValue.compare(pos, parameterName.size(), parameterName) == 0;
        if (matchingName)
        {
            pos += parameterName.size();
            while (pos < headerValue.size() &&
                   (headerValue[pos] == ' ' || headerValue[pos] == '\t'))
                ++pos;
        }
        else
        {
            while (pos < headerValue.size() && headerValue[pos] != '=' &&
                   headerValue[pos] != ';')
                ++pos;
        }
        if (pos == headerValue.size() || headerValue[pos] != '=')
            continue;

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
            // Honor quoted-pair escapes so `\"` does not end the value, as the
            // other header splitters do. Disagreeing on where a value ends
            // lets a crafted header mean different things to drogon and to a
            // proxy in front of it. The value is returned raw, escapes kept.
            valueBegin = ++pos;
            while (pos < headerValue.size() && headerValue[pos] != '"')
                pos +=
                    (headerValue[pos] == '\\' && pos + 1 < headerValue.size())
                        ? 2
                        : 1;
            if (pos >= headerValue.size())
            {
                return false;
            }
            valueEnd = pos;
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

        if (matchingName)
        {
            parameterValue =
                headerValue.substr(valueBegin, valueEnd - valueBegin);
            return true;
        }
    }
    return false;
}

inline bool parseMultipartBoundary(std::string_view contentType,
                                   std::string_view &boundary)
{
    constexpr std::string_view mediaType = "multipart/form-data";
    constexpr std::string_view boundaryName = "boundary";
    const auto semi = contentType.find(';');
    if (semi == std::string_view::npos)
        return false;
    const auto type = utils::trim(contentType.substr(0, semi));
    if (type.size() != mediaType.size() ||
        (type != mediaType && !startsWithIgnoreCase(type, mediaType)))
        return false;

    const auto isSpace = [](char c) { return c == ' ' || c == '\t'; };
    size_t pos = semi + 1;
    bool found = false;
    while (pos < contentType.size())
    {
        while (pos < contentType.size() && isSpace(contentType[pos]))
            ++pos;
        const auto nameBegin = pos;
        while (pos < contentType.size() && contentType[pos] != '=' &&
               contentType[pos] != ';')
            ++pos;
        if (pos == contentType.size() || contentType[pos] != '=')
            return false;

        auto nameEnd = pos;
        while (nameEnd > nameBegin && isSpace(contentType[nameEnd - 1]))
            --nameEnd;
        const bool isBoundary =
            nameEnd - nameBegin == boundaryName.size() &&
            startsWithIgnoreCase(contentType.substr(nameBegin,
                                                    nameEnd - nameBegin),
                                 boundaryName);

        ++pos;
        while (pos < contentType.size() && isSpace(contentType[pos]))
            ++pos;
        const bool quotedValue =
            pos < contentType.size() && contentType[pos] == '"';
        size_t valueBegin = pos;
        size_t valueEnd;
        if (quotedValue)
        {
            valueBegin = ++pos;
            while (pos < contentType.size())
            {
                if (contentType[pos] == '\\' && pos + 1 < contentType.size())
                    pos += 2;
                else if (contentType[pos] == '"')
                    break;
                else
                    ++pos;
            }
            if (pos == contentType.size())
                return false;
            valueEnd = pos++;
            while (pos < contentType.size() && isSpace(contentType[pos]))
                ++pos;
            if (pos < contentType.size() && contentType[pos] != ';')
                return false;
        }
        else
        {
            bool inQuotes = false;
            while (pos < contentType.size())
            {
                const char c = contentType[pos];
                if (inQuotes && c == '\\' && pos + 1 < contentType.size())
                    pos += 2;
                else if (c == '"')
                {
                    inQuotes = !inQuotes;
                    ++pos;
                }
                else if (c == ';' && !inQuotes)
                    break;
                else
                    ++pos;
            }
            if (inQuotes)
                return false;
            valueEnd = pos;
            while (valueEnd > valueBegin && isSpace(contentType[valueEnd - 1]))
                --valueEnd;
        }
        if (isBoundary)
        {
            const auto value =
                contentType.substr(valueBegin, valueEnd - valueBegin);
            if (found || value.empty() || value.size() > 70 ||
                value.back() == ' ')
                return false;
            for (const unsigned char c : value)
            {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '\'' || c == '+' ||
                    c == '_' || c == '-' || c == '.')
                    continue;
                if (quotedValue &&
                    (c == '(' || c == ')' || c == ',' || c == '/' || c == ':' ||
                     c == '=' || c == '?' || c == ' '))
                    continue;
                return false;
            }
            boundary = value;
            found = true;
        }
        if (pos < contentType.size())
            ++pos;
    }
    return found;
}

}  // namespace internal
class Cookie;

const std::string_view &contentTypeToMime(ContentType contentType);
const std::string_view &statusCodeToString(int code);
ContentType getContentType(const std::string &fileName);
ContentType parseContentType(const std::string_view &contentType);
FileType parseFileType(const std::string_view &fileExtension);
FileType getFileType(ContentType contentType);
void registerCustomExtensionMime(const std::string &ext,
                                 const std::string &mime);
const std::string_view fileNameToMime(const std::string &fileName);
std::pair<ContentType, const std::string_view> fileNameToContentTypeAndMime(
    const std::string &filename);

inline std::string_view getFileExtension(const std::string &fileName)
{
    auto pos = fileName.rfind('.');
    if (pos == std::string::npos)
        return "";
    return std::string_view(&fileName[pos + 1], fileName.length() - pos - 1);
}

const std::vector<std::string_view> &getFileExtensions(ContentType contentType);

/// Return false (and log an error) if the header field or value contains a CR
/// or LF character, which are not allowed in HTTP field names and values
/// (RFC 9110, section 5.5).
bool isValidHeader(std::string_view field, std::string_view value);

/// Return false (and log an error) if the cookie key or value contains a CR or
/// LF character.
bool isValidCookie(std::string_view key, std::string_view value);

/// Return false (and log an error) if the cookie key, value, domain or path
/// contains a CR or LF character.
bool isValidCookie(const Cookie &cookie);

/// Return true if the string contains a CR or LF character, which is not
/// allowed in HTTP field names or values (RFC 9110, section 5.5).
bool containsCRLF(std::string_view str);

/// Canonicalize a decoded request path: collapse empty and "." segments and
/// resolve "..", keeping a trailing slash. A path that is already canonical
/// is detected in a single scan and returned as a plain copy. Returns nullopt
/// if the path contains NUL, climbs above the root or names a root (a Windows
/// drive or UNC share).
std::optional<std::string> canonicalizePath(std::string_view path);

namespace internal
{
// A view over delimiter-separated header values. Delimiters inside quoted
// strings are kept in the value. next() returns nullopt at the end; an empty
// string_view represents an empty field.
class HttpHeaderFieldIterator
{
  public:
    HttpHeaderFieldIterator(std::string_view value, char delimiter)
        : remaining_(value), delimiter_(delimiter)
    {
    }

    std::optional<std::string_view> next();

  private:
    std::string_view remaining_;
    char delimiter_;
    bool done_{false};
};

// Parameter values are returned as raw views, including any quotes. This is
// a splitter, not a validator or a quoted-string decoder.
class HttpHeaderParameterIterator
{
  public:
    explicit HttpHeaderParameterIterator(HttpHeaderFieldIterator fields)
        : fields_(fields)
    {
    }

    std::optional<std::pair<std::string_view, std::string_view>> next();

  private:
    HttpHeaderFieldIterator fields_;
};
}  // namespace internal

internal::HttpHeaderFieldIterator iterateHeaderField(std::string_view value);

// Split a master value such as "text/plain" from its semicolon-separated
// parameters. The returned views refer to the original header value.
std::pair<std::string_view, internal::HttpHeaderParameterIterator>
iterateHeaderParameters(std::string_view value);

/// Return true if a comma-separated HTTP field contains the given token.
bool containsHttpToken(std::string_view value, std::string_view token);

inline const std::vector<std::string_view> &getFileExtensions(
    const std::string_view &contentType)
{
    return getFileExtensions(parseContentType(contentType));
}

template <typename T>
inline constexpr const char *contentLengthFormatString()
{
    return "content-length: %d\r\n";
}

template <>
inline constexpr const char *contentLengthFormatString<unsigned int>()
{
    return "content-length: %u\r\n";
}

template <>
inline constexpr const char *contentLengthFormatString<long>()
{
    return "content-length: %ld\r\n";
}

template <>
inline constexpr const char *contentLengthFormatString<unsigned long>()
{
    return "content-length: %lu\r\n";
}

template <>
inline constexpr const char *contentLengthFormatString<long long>()
{
    return "content-length: %lld\r\n";
}

template <>
inline constexpr const char *contentLengthFormatString<unsigned long long>()
{
    return "content-length: %llu\r\n";
}
}  // namespace drogon
