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
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
#include <string_view>
#else
#include <boost/utility/string_view.hpp>
#include <boost/functional/hash.hpp>
#endif

#include <trantor/utils/LogStream.h>

namespace drogon
{
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
using std::string_view;
#else
using boost::string_view;
#endif
}  // namespace drogon
namespace trantor
{
inline LogStream &operator<<(LogStream &ls, const drogon::string_view &v)
{
    ls.append(v.data(), v.length());
    return ls;
}
}  // namespace trantor

#if __cplusplus < 201703L
namespace std
{
template <>
struct hash<drogon::string_view>
{
    size_t operator()(const drogon::string_view &__str) const noexcept
    {
        // Take from the memory header file
        //===-------------------------- memory
        //------------------------------------===//
        //
        //                     The LLVM Compiler Infrastructure
        //
        // This file is dual licensed under the MIT and the University of
        // Illinois Open Source Licenses. See LICENSE.TXT for details.
        //
        //===----------------------------------------------------------------------===//
        const size_t __m = 0x5bd1e995;
        const size_t __r = 24;
        size_t __h = __str.length();
        auto __len = __h;
        const unsigned char *__data = (const unsigned char *)(__str.data());
        for (; __len >= 4; __data += 4, __len -= 4)
        {
            size_t __k = *((size_t *)__data);
            __k *= __m;
            __k ^= __k >> __r;
            __k *= __m;
            __h *= __m;
            __h ^= __k;
        }
        switch (__len)
        {
            case 3:
                __h ^= __data[2] << 16;
            case 2:
                __h ^= __data[1] << 8;
            case 1:
                __h ^= __data[0];
                __h *= __m;
        }
        __h ^= __h >> 13;
        __h *= __m;
        __h ^= __h >> 15;
        return __h;
    }
};
}  // namespace std

#endif
