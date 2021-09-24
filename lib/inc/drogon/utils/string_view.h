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
#include <stdint.h>
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

#if __cplusplus < 201703L && !(defined _MSC_VER && _MSC_VER > 1900)
namespace drogon
{
#ifndef _MSC_VER
template <size_t N>
struct StringViewHasher;

template <>
struct StringViewHasher<4>
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
        const uint32_t __m = 0x5bd1e995;
        const uint32_t __r = 24;
        uint32_t __h = __str.length();
        auto __len = __h;
        const unsigned char *__data = (const unsigned char *)(__str.data());
        for (; __len >= 4; __data += 4, __len -= 4)
        {
            uint32_t __k = *((uint32_t *)__data);
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

template <>
struct StringViewHasher<8>
{
    static const size_t __k0 = 0xc3a5c85c97cb3127ULL;
    static const size_t __k1 = 0xb492b66fbe98f273ULL;
    static const size_t __k2 = 0x9ae16a3b2f90404fULL;
    static const size_t __k3 = 0xc949d7c7509e6557ULL;

    static size_t __rotate(size_t __val, int __shift)
    {
        return __shift == 0 ? __val
                            : ((__val >> __shift) | (__val << (64 - __shift)));
    }

    static size_t __rotate_by_at_least_1(size_t __val, int __shift)
    {
        return (__val >> __shift) | (__val << (64 - __shift));
    }

    static size_t __shift_mix(size_t __val)
    {
        return __val ^ (__val >> 47);
    }

    static size_t __hash_len_16(size_t __u, size_t __v)
    {
        const size_t __mul = 0x9ddfea08eb382d69ULL;
        size_t __a = (__u ^ __v) * __mul;
        __a ^= (__a >> 47);
        size_t __b = (__v ^ __a) * __mul;
        __b ^= (__b >> 47);
        __b *= __mul;
        return __b;
    }
    template <class Size>
    inline static Size __loadword(const void *__p)
    {
        Size __r;
        std::memcpy(&__r, __p, sizeof(__r));
        return __r;
    }

    static size_t __hash_len_0_to_16(const char *__s, size_t __len)
    {
        if (__len > 8)
        {
            const size_t __a = __loadword<size_t>(__s);
            const size_t __b = __loadword<size_t>(__s + __len - 8);
            return __hash_len_16(__a,
                                 __rotate_by_at_least_1(__b + __len, __len)) ^
                   __b;
        }
        if (__len >= 4)
        {
            const uint32_t __a = __loadword<uint32_t>(__s);
            const uint32_t __b = __loadword<uint32_t>(__s + __len - 4);
            return __hash_len_16(__len + (__a << 3), __b);
        }
        if (__len > 0)
        {
            const unsigned char __a = __s[0];
            const unsigned char __b = __s[__len >> 1];
            const unsigned char __c = __s[__len - 1];
            const uint32_t __y =
                static_cast<uint32_t>(__a) + (static_cast<uint32_t>(__b) << 8);
            const uint32_t __z = __len + (static_cast<uint32_t>(__c) << 2);
            return __shift_mix(__y * __k2 ^ __z * __k3) * __k2;
        }
        return __k2;
    }

    static size_t __hash_len_17_to_32(const char *__s, size_t __len)
    {
        const size_t __a = __loadword<size_t>(__s) * __k1;
        const size_t __b = __loadword<size_t>(__s + 8);
        const size_t __c = __loadword<size_t>(__s + __len - 8) * __k2;
        const size_t __d = __loadword<size_t>(__s + __len - 16) * __k0;
        return __hash_len_16(__rotate(__a - __b, 43) + __rotate(__c, 30) + __d,
                             __a + __rotate(__b ^ __k3, 20) - __c + __len);
    }

    // Return a 16-byte hash for 48 bytes.  Quick and dirty.
    // Callers do best to use "random-looking" values for a and b.
    static std::pair<size_t, size_t> __weak_hash_len_32_with_seeds(size_t __w,
                                                                   size_t __x,
                                                                   size_t __y,
                                                                   size_t __z,
                                                                   size_t __a,
                                                                   size_t __b)
    {
        __a += __w;
        __b = __rotate(__b + __a + __z, 21);
        const size_t __c = __a;
        __a += __x;
        __a += __y;
        __b += __rotate(__a, 44);
        return std::pair<size_t, size_t>(__a + __z, __b + __c);
    }

    // Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
    static std::pair<size_t, size_t> __weak_hash_len_32_with_seeds(
        const char *__s,
        size_t __a,
        size_t __b)
    {
        return __weak_hash_len_32_with_seeds(__loadword<size_t>(__s),
                                             __loadword<size_t>(__s + 8),
                                             __loadword<size_t>(__s + 16),
                                             __loadword<size_t>(__s + 24),
                                             __a,
                                             __b);
    }

    // Return an 8-byte hash for 33 to 64 bytes.
    static size_t __hash_len_33_to_64(const char *__s, size_t __len)
    {
        size_t __z = __loadword<size_t>(__s + 24);
        size_t __a = __loadword<size_t>(__s) +
                     (__len + __loadword<size_t>(__s + __len - 16)) * __k0;
        size_t __b = __rotate(__a + __z, 52);
        size_t __c = __rotate(__a, 37);
        __a += __loadword<size_t>(__s + 8);
        __c += __rotate(__a, 7);
        __a += __loadword<size_t>(__s + 16);
        size_t __vf = __a + __z;
        size_t __vs = __b + __rotate(__a, 31) + __c;
        __a =
            __loadword<size_t>(__s + 16) + __loadword<size_t>(__s + __len - 32);
        __z += __loadword<size_t>(__s + __len - 8);
        __b = __rotate(__a + __z, 52);
        __c = __rotate(__a, 37);
        __a += __loadword<size_t>(__s + __len - 24);
        __c += __rotate(__a, 7);
        __a += __loadword<size_t>(__s + __len - 16);
        size_t __wf = __a + __z;
        size_t __ws = __b + __rotate(__a, 31) + __c;
        size_t __r = __shift_mix((__vf + __ws) * __k2 + (__wf + __vs) * __k0);
        return __shift_mix(__r * __k0 + __vs) * __k2;
    }
    size_t operator()(const drogon::string_view &__str) const noexcept
    {
        size_t __len = __str.length();
        const char *__s = static_cast<const char *>(__str.data());
        if (__len <= 32)
        {
            if (__len <= 16)
            {
                return __hash_len_0_to_16(__s, __len);
            }
            else
            {
                return __hash_len_17_to_32(__s, __len);
            }
        }
        else if (__len <= 64)
        {
            return __hash_len_33_to_64(__s, __len);
        }

        // For strings over 64 bytes we hash the end first, and then as we
        // loop we keep 56 bytes of state: v, w, x, y, and z.
        size_t __x = __loadword<size_t>(__s + __len - 40);
        size_t __y = __loadword<size_t>(__s + __len - 16) +
                     __loadword<size_t>(__s + __len - 56);
        size_t __z = __hash_len_16(__loadword<size_t>(__s + __len - 48) + __len,
                                   __loadword<size_t>(__s + __len - 24));
        std::pair<size_t, size_t> __v =
            __weak_hash_len_32_with_seeds(__s + __len - 64, __len, __z);
        std::pair<size_t, size_t> __w =
            __weak_hash_len_32_with_seeds(__s + __len - 32, __y + __k1, __x);
        __x = __x * __k1 + __loadword<size_t>(__s);

        // Decrease len to the nearest multiple of 64, and operate on 64-byte
        // chunks.
        __len = (__len - 1) & ~static_cast<size_t>(63);
        do
        {
            __x = __rotate(__x + __y + __v.first + __loadword<size_t>(__s + 8),
                           37) *
                  __k1;
            __y =
                __rotate(__y + __v.second + __loadword<size_t>(__s + 48), 42) *
                __k1;
            __x ^= __w.second;
            __y += __v.first + __loadword<size_t>(__s + 40);
            __z = __rotate(__z + __w.first, 33) * __k1;
            __v = __weak_hash_len_32_with_seeds(__s,
                                                __v.second * __k1,
                                                __x + __w.first);
            __w = __weak_hash_len_32_with_seeds(
                __s + 32, __z + __w.second, __y + __loadword<size_t>(__s + 16));
            std::swap(__z, __x);
            __s += 64;
            __len -= 64;
        } while (__len != 0);
        return __hash_len_16(__hash_len_16(__v.first, __w.first) +
                                 __shift_mix(__y) * __k1 + __z,
                             __hash_len_16(__v.second, __w.second) + __x);
    }
};
#else

// Taken and modified from https://stackoverflow.com/a/8317622/3687637
struct ShortStringViewHasher
{
    size_t operator()(const string_view &sv) const
    {
        const size_t initValue = 1049039;
        const size_t A = 6665339;
        const size_t B = 2534641;
        size_t h = initValue;
        for (char ch : sv)
            h = (h * A) ^ (ch * B);
        return h;
    }
};
#endif
}  // namespace drogon
namespace std
{
template <>
struct hash<drogon::string_view>
{
    size_t operator()(const drogon::string_view &__str) const noexcept
    {
        // MSVC is having problems with non-aligned strings
#ifndef _MSC_VER
        return drogon::StringViewHasher<sizeof(size_t)>()(__str);
#else
        return drogon::ShortStringViewHasher(__str);
#endif
    }
};
}  // namespace std

#endif
