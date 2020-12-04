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
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
#include <optional>
#else
#include <boost/optional.hpp>
#endif

namespace drogon
{
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
using std::optional;
#else
using boost::optional;
#endif
}  // namespace drogon