/**
 *
 *  HttpUtils.h
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

#pragma once

#include <drogon/utils/string_view.h>
#include <drogon/HttpTypes.h>
#include <string>
#include <trantor/utils/MsgBuffer.h>

namespace drogon
{
ContentType parseContentType(const string_view &contentType);
const string_view &webContentTypeToString(ContentType contenttype);
const string_view &statusCodeToString(int code);
ContentType getContentType(const std::string &fileName);
template <typename T>
inline constexpr const char *contentLengthFormatString()
{
    return "Content-Length: %d\r\n";
}
template <>
inline constexpr const char *contentLengthFormatString<unsigned int>()
{
    return "Content-Length: %u\r\n";
}
template <>
inline constexpr const char *contentLengthFormatString<long>()
{
    return "Content-Length: %ld\r\n";
}
template <>
inline constexpr const char *contentLengthFormatString<unsigned long>()
{
    return "Content-Length: %lu\r\n";
}
template <>
inline constexpr const char *contentLengthFormatString<long long>()
{
    return "Content-Length: %lld\r\n";
}
template <>
inline constexpr const char *contentLengthFormatString<unsigned long long>()
{
    return "Content-Length: %llu\r\n";
}
}  // namespace drogon
