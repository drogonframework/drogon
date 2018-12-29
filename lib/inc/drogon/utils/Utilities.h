/**
 *
 *  Utilities.h
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

#include <drogon/config.h>
#include <trantor/utils/Date.h>
#include <string>
#include <vector>

namespace drogon
{
/// Determine if the string is an integer
bool isInteger(const std::string &str);

/// Generate random a string
/**
 * @param length: The string length
 * The returned string consists of uppercase and lowercase letters and numbers
 */
std::string genRandomString(int length);

/// Convert a binary string to hex format
std::string binaryStringToHex(const unsigned char *ptr, size_t length);

/// Get a binary string from hexadecimal format
std::string hexToBinaryString(const char *ptr, size_t length);

/// Split the string into multiple separated strings.
std::vector<std::string> splitString(const std::string &str, const std::string &separator);

/// Get UUID string.
std::string getuuid();

/// Encode the string to base64 format.
std::string base64Encode(const unsigned char *bytes_to_encode, unsigned int in_len);

/// Decode the base64 format string.
std::string base64Decode(std::string const &encoded_string);

/// Decode the URL format string send by web browser.
std::string urlDecode(const std::string &szToDecode);

/// Commpress or decompress data using gzip lib.
/**
 * @param data: Data before compressing or after decompressing
 * @param ndata: Data length before compressing or after decompressing
 * @param zdata: Data after compressing or before decompressing
 * @param nzdata: Data length after compressing or before decompressing
 */
int gzipCompress(const char *data, const size_t ndata,
                 char *zdata, size_t *nzdata);
int gzipDecompress(const char *zdata, const size_t nzdata,
                   char *data, size_t *ndata);

/// Get the http full date string
/**
 * rfc2616-3.3.1
 * Full Date format(RFC 822)
 * like this:Sun, 06 Nov 1994 08:49:37 GMT
 *           Wed, 12 Sep 2018 09:22:40 GMT
 */
char *getHttpFullDate(const trantor::Date &date = trantor::Date::now(), bool *isChanged = nullptr);

/// Get a formatted string
std::string formattedString(const char *format, ...);

/// Recursively create a file system path
/**
 * Returns 0 or -1 on success or failure.
 */
int createPath(const std::string &path);

} // namespace drogon
