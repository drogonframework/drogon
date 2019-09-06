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
#endif
namespace drogon
{
#if __cplusplus >= 201703L
using std::string_view;
#else
using boost::string_view;
#endif
}  // namespace drogon
