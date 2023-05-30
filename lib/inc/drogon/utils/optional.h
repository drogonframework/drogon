/**
 *
 *  optional.h
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
#include <optional>
#else
#include <boost/optional.hpp>
#endif

namespace drogon
{
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
using std::nullopt;
using std::optional;
#else
const boost::none_t nullopt = boost::none;
using boost::optional;
#endif
}  // namespace drogon
