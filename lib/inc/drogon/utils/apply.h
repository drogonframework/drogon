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
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
#include <tuple>
#else
#include <boost/hana/fwd/unpack.hpp>
#endif

namespace drogon
{
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
    using std::apply;
#else
    template <class F, class Tuple>
    constexpr decltype(auto) apply(F&& f, Tuple&& t)
    {
        return boost::hana::unpack(std::move(t), std::move(f));
    }
#endif
}  // namespace drogon