/**
 *
 *  string_view.h
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
#if __cplusplus >= 201703L
#include <string_view>
#else
#include <boost/utility/string_view.hpp>
#include <boost/functional/hash.hpp>
#endif
namespace drogon
{
#if __cplusplus >= 201703L
using std::string_view;
#else
using boost::string_view;
#endif
}  // namespace drogon

#if __cplusplus < 201703L
namespace std
{
template <>
struct hash<drogon::string_view>
{
    size_t operator()(const drogon::string_view &v) const
    {
        return boost::hash_value(v);
    }
};
}  // namespace std
#include <trantor/utils/LogStream.h>
namespace trantor
{
inline LogStream &operator<<(LogStream &ls, const drogon::string_view &v)
{
    ls.append(v.data(), v.length());
    return ls;
}
}  // namespace trantor
#endif
