/**
 *
 *  package.h
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
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#include <tuple>
#else
#include <utility>
namespace
{
template <typename F, typename Tuple, std::size_t... I>
constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>)
{
    return static_cast<F &&>(f)(std::get<I>(static_cast<Tuple &&>(t))...);
}
}  // anonymous namespace
#endif

namespace drogon
{
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
using std::apply;
#else
template <typename F, typename Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
    return apply_impl(
        std::forward<F>(f),
        std::forward<Tuple>(t),
        std::make_index_sequence<
            std::tuple_size<std::remove_reference_t<Tuple> >::value>{});
}
#endif
}  // namespace drogon