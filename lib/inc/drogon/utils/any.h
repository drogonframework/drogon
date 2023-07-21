/**
 *
 *  any.h
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
#include <any>
#else
#include <boost/any.hpp>
#endif

namespace drogon
{
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
using std::any;
using std::any_cast;
#else
using boost::any;
using boost::any_cast;
#endif
}  // namespace drogon
