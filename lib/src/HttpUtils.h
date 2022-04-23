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

#include <drogon/utils/string_view.h>
#include <drogon/HttpTypes.h>
#include <string>
#include <trantor/utils/MsgBuffer.h>

namespace drogon
{
const string_view &contentTypeToMime(ContentType contentType);
const string_view &statusCodeToString(int code);
ContentType getContentType(const std::string &fileName);
ContentType parseContentType(const string_view &contentType);
FileType parseFileType(const string_view &fileExtension);
void registerCustomExtensionMime(const std::string &ext,
                                 const std::string &mime);
const string_view fileNameToMime(const std::string &fileName);
std::pair<ContentType, const string_view> fileNameToContentTypeAndMime(
    const std::string &filename);
inline string_view getFileExtension(const std::string &fileName)
{
    auto pos = fileName.rfind('.');
    if (pos == std::string::npos)
        return "";
    return string_view(&fileName[pos + 1], fileName.length() - pos - 1);
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
