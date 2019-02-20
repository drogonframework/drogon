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
#endif
namespace drogon
{
#if CXX_STD >= 17
const std::string_view &webContentTypeToString(ContentType contenttype);
#else
const char *webContentTypeToString(ContentType contenttype);
#endif

//std::string webContentTypeAndCharsetToString(ContentType contenttype, const std::string &charSet);

} // namespace drogon
