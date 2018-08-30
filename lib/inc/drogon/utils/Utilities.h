/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <uuid/uuid.h>
#include <string>
#include <vector>
namespace drogon{
    bool isInteger(const std::string &str);
    std::string genRandomString(int length);
    std::string stringToHex(unsigned char* ptr, long long length);
    std::vector<std::string> splitString(const std::string &str,const std::string &separator);
    std::string getuuid();
    std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
    std::string base64_decode(std::string const& encoded_string);
    std::string urlDecode(const std::string& szToDecode);
  //  std::string base64Encode(char *in_str, int in_len)
}



