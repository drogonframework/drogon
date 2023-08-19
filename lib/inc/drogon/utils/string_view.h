/**
 *
 *  @file string_view.h
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
#include <string_view>
#include <trantor/utils/LogStream.h>

namespace drogon
{
using std::string_view;
}  // namespace drogon
namespace trantor
{
inline LogStream &operator<<(LogStream &ls, const drogon::string_view &v)
{
    ls.append(v.data(), v.length());
    return ls;
}
}  // namespace trantor
