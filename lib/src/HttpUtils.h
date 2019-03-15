/**
 *
 *  HttpUtils.h
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
#include <string>
#include <drogon/HttpTypes.h>
#include <drogon/config.h>
#if CXX_STD >= 17
#include <string_view>
typedef std::string_view string_view;
#else
#include <experimental/string_view>
typedef std::experimental::basic_string_view<char> string_view;
#endif
namespace drogon
{

const string_view &webContentTypeToString(ContentType contenttype);
const string_view &statusCodeToString(int code);

} // namespace drogon
