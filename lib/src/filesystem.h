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

#include <drogon/config.h>
#include <trantor/utils/LogStream.h>

#if HAS_STD_FILESYSTEM_PATH
#include <filesystem>
#include <system_error>
#else
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#endif

namespace drogon
{
#if HAS_STD_FILESYSTEM_PATH
namespace filesystem = std::filesystem;
using std::error_code;
#else
namespace filesystem = boost::filesystem;
using boost::system::error_code;
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
