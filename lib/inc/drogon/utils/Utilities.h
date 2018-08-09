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
}



