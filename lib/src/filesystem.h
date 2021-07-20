/*  @author An Tao
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

/* Check of std::filesystem::path availability:
 * - OS X: depends on the target OSX version (>= 10.15 Catalina)
 * - Windows: Visual Studio >= 2019 (c++20)
 * - Others: should already have it in c++17
 */
#if (defined(__APPLE__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500L) || \
    (defined(_WIN32) && defined(_MSC_VER) && _MSC_VER >= 1920) ||        \
    (!defined(__APPLE__) && !defined(_WIN32) && __cplusplus >= 201703L)
#define HAS_STD_FILESYSTEM_PATH
#endif

#include <trantor/utils/LogStream.h>

#ifdef HAS_STD_FILESYSTEM_PATH
#include <filesystem>
#else
#include <boost/filesystem.hpp>
#endif

namespace drogon
{
#ifdef HAS_STD_FILESYSTEM_PATH
namespace filesystem = std::filesystem;
#else
namespace filesystem = boost::filesystem;
#endif
}  // namespace drogon

namespace trantor
{
inline LogStream &operator<<(LogStream &ls, const drogon::filesystem::path &v)
{
#if defined(_WIN32) && defined(__cpp_char8_t)
    // Convert UCS-2 to UTF-8, not ASCII - not needed on other OSes
    auto u8path{v.u8string()};
    ls.append((const char *)u8path.data(), u8path.length());
#else
    // No need to convert
    ls.append(v.string().data(), v.string().length());
#endif
    return ls;
}
}  // namespace trantor
