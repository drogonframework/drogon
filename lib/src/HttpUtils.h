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
#include <drogon/HttpTypes.h>
#include <string>
#include <string_view>

namespace drogon
{
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
