/**
 *
 *  @file Utilities.h
 *  @author An Tao
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

#include <drogon/exports.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/Funcs.h>
#include <trantor/utils/Utilities.h>
#include <drogon/utils/string_view.h>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include <sstream>
#include <algorithm>
#ifdef _WIN32
#include <time.h>
DROGON_EXPORT char *strptime(const char *s, const char *f, struct tm *tm);
DROGON_EXPORT time_t timegm(struct tm *tm);
#endif
namespace drogon
{
namespace internal
{
template <typename T>
struct CanConvertFromStringStream
{
  private:
    using yes = std::true_type;
    using no = std::false_type;

    template <typename U>
    static auto test(U *p, std::stringstream &&ss)
        -> decltype((ss >> *p), yes());

    template <typename>
    static no test(...);

  public:
    static constexpr bool value =
        std::is_same<decltype(test<T>(nullptr, std::stringstream())),
                     yes>::value;
};
}  // namespace internal
namespace utils
{
/// Determine if the string is an integer
DROGON_EXPORT bool isInteger(const std::string &str);
/// Determine if the string is an integer
DROGON_EXPORT bool isInteger(string_view str);

/// Determine if the string is base64 encoded
DROGON_EXPORT bool isBase64(const std::string &str);
/// Determine if the string is base64 encoded
DROGON_EXPORT bool isBase64(string_view str);

/// Generate random a string
/**
 * @param length The string length
 * The returned string consists of uppercase and lowercase letters and numbers
 */
DROGON_EXPORT std::string genRandomString(int length);

/// Convert a binary string to hex format
DROGON_EXPORT std::string binaryStringToHex(const unsigned char *ptr,
                                            size_t length);

/// Get a binary string from hexadecimal format
DROGON_EXPORT std::string hexToBinaryString(const char *ptr, size_t length);

/// Get a binary vector from hexadecimal format
DROGON_EXPORT std::vector<char> hexToBinaryVector(const char *ptr,
                                                  size_t length);

/// Split the string into multiple separated strings.
/**
 * @param acceptEmptyString if true, empty strings are accepted in the
 * result, for example, splitting the ",1,2,,3," by "," produces
 * ["","1","2","","3",""]
 */
inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &separator,
                                            bool acceptEmptyString = false)
{
    return trantor::splitString(str, separator, acceptEmptyString);
}

DROGON_EXPORT std::set<std::string> splitStringToSet(
    const std::string &str,
    const std::string &separator);

/// Get UUID string.
DROGON_EXPORT std::string getUuid();

/// Get the encoded length of base64.
constexpr size_t base64EncodedLength(unsigned int in_len, bool padded = true)
{
    return padded ? ((in_len + 3 - 1) / 3) * 4 : (in_len * 8 + 6 - 1) / 6;
}

/// Encode the string to base64 format.
DROGON_EXPORT std::string base64Encode(const unsigned char *bytes_to_encode,
                                       unsigned int in_len,
                                       bool url_safe = false,
                                       bool padded = true);

/// Encode the string to base64 format.
inline std::string base64Encode(string_view data,
                                bool url_safe = false,
                                bool padded = true)
{
    return base64Encode((unsigned char *)data.data(),
                        data.size(),
                        url_safe,
                        padded);
}

/// Encode the string to base64 format with no padding.
inline std::string base64EncodeUnpadded(const unsigned char *bytes_to_encode,
                                        unsigned int in_len,
                                        bool url_safe = false)
{
    return base64Encode(bytes_to_encode, in_len, url_safe, false);
}

/// Encode the string to base64 format with no padding.
inline std::string base64EncodeUnpadded(string_view data, bool url_safe = false)
{
    return base64Encode(data, url_safe, false);
}

/// Get the decoded length of base64.
constexpr size_t base64DecodedLength(unsigned int in_len)
{
    return (in_len * 3) / 4;
}

/// Decode the base64 format string.
DROGON_EXPORT std::string base64Decode(string_view encoded_string);
DROGON_EXPORT std::vector<char> base64DecodeToVector(
    string_view encoded_string);

/// Check if the string need decoding
DROGON_EXPORT bool needUrlDecoding(const char *begin, const char *end);

/// Decode from or encode to the URL format string
DROGON_EXPORT std::string urlDecode(const char *begin, const char *end);
inline std::string urlDecode(const std::string &szToDecode)
{
    auto begin = szToDecode.data();
    return urlDecode(begin, begin + szToDecode.length());
}
inline std::string urlDecode(const string_view &szToDecode)
{
    auto begin = szToDecode.data();
    return urlDecode(begin, begin + szToDecode.length());
}

DROGON_EXPORT std::string urlEncode(const std::string &);
DROGON_EXPORT std::string urlEncodeComponent(const std::string &);

/// Get the MD5 digest of a string.
DROGON_EXPORT std::string getMd5(const char *data, const size_t dataLen);
inline std::string getMd5(const std::string &originalString)
{
    return getMd5(originalString.data(), originalString.length());
}

DROGON_EXPORT std::string getSha1(const char *data, const size_t dataLen);
inline std::string getSha1(const std::string &originalString)
{
    return getSha1(originalString.data(), originalString.length());
}

DROGON_EXPORT std::string getSha256(const char *data, const size_t dataLen);
inline std::string getSha256(const std::string &originalString)
{
    return getSha256(originalString.data(), originalString.length());
}

DROGON_EXPORT std::string getSha3(const char *data, const size_t dataLen);
inline std::string getSha3(const std::string &originalString)
{
    return getSha3(originalString.data(), originalString.length());
}

DROGON_EXPORT std::string getBlake2b(const char *data, const size_t dataLen);
inline std::string getBlake2b(const std::string &originalString)
{
    return getBlake2b(originalString.data(), originalString.length());
}

/// Commpress or decompress data using gzip lib.
/**
 * @param data the input data
 * @param ndata the input data length
 */
DROGON_EXPORT std::string gzipCompress(const char *data, const size_t ndata);
DROGON_EXPORT std::string gzipDecompress(const char *data, const size_t ndata);

/// Commpress or decompress data using brotli lib.
/**
 * @param data the input data
 * @param ndata the input data length
 */
DROGON_EXPORT std::string brotliCompress(const char *data, const size_t ndata);
DROGON_EXPORT std::string brotliDecompress(const char *data,
                                           const size_t ndata);

/// Get the http full date string
/**
 * rfc2616-3.3.1
 * Full Date format(RFC 822)
 * like this:
 * @code
   Sun, 06 Nov 1994 08:49:37 GMT
   Wed, 12 Sep 2018 09:22:40 GMT
   @endcode
 */
DROGON_EXPORT char *getHttpFullDate(
    const trantor::Date &date = trantor::Date::now());

/// Get the trantor::Date object according to the http full date string
/**
 * Returns trantor::Date(std::numeric_limits<int64_t>::max()) upon failure.
 */
DROGON_EXPORT trantor::Date getHttpDate(const std::string &httpFullDateString);

/// Get a formatted string
DROGON_EXPORT std::string formattedString(const char *format, ...);

/// Recursively create a file system path
/**
 * Return 0 or -1 on success or failure.
 */
DROGON_EXPORT int createPath(const std::string &path);

/**
 * @details Convert a wide string path with arbitrary directory separators
 * to a UTF-8 portable path for use with trantor.
 *
 * This is a helper, mainly for Windows and multi-platform projects.
 *
 * @note On Windows, backslash directory separators are converted to slash to
 * keep portable paths.
 *
 * @remarks On other OSes, backslashes are not converted to slash, since they
 * are valid characters for directory/file names.
 *
 * @param strPath Wide string path.
 *
 * @return std::string UTF-8 path, with slash directory separator.
 */
inline std::string fromWidePath(const std::wstring &strPath)
{
    return trantor::utils::fromWidePath(strPath);
}

/**
 * @details Convert a UTF-8 path with arbitrary directory separator to a wide
 * string path.
 *
 * This is a helper, mainly for Windows and multi-platform projects.
 *
 * @note On Windows, slash directory separators are converted to backslash.
 * Although it accepts both slash and backslash as directory separator in its
 * API, it is better to stick to its standard.

 * @remarks On other OSes, slashes are not converted to backslashes, since they
 * are not interpreted as directory separators and are valid characters for
 * directory/file names.
 *
 * @param strUtf8Path Ascii path considered as being UTF-8.
 *
 * @return std::wstring path with, on windows, standard backslash directory
 * separator to stick to its standard.
 */
inline std::wstring toWidePath(const std::string &strUtf8Path)
{
    return trantor::utils::toWidePath(strUtf8Path);
}

/**
 * @brief Convert a generic (UTF-8) path with to an OS native path.
 * @details This is a helper, mainly for Windows and multi-platform projects.
 *
 * On Windows, slash directory separators are converted to backslash, and a
 * wide string is returned.
 *
 * On other OSes, returns an UTF-8 string _without_ altering the directory
 * separators.
 *
 * @param strPath Wide string or UTF-8 path.
 *
 * @return An OS path, suitable for use with the OS API.
 */
#if defined(_WIN32) && !defined(__MINGW32__)
inline std::wstring toNativePath(const std::string &strPath)
{
    return trantor::utils::toNativePath(strPath);
}
inline const std::wstring &toNativePath(const std::wstring &strPath)
{
    return trantor::utils::toNativePath(strPath);
}
#else   // __WIN32
inline const std::string &toNativePath(const std::string &strPath)
{
    return trantor::utils::toNativePath(strPath);
}
inline std::string toNativePath(const std::wstring &strPath)
{
    return trantor::utils::toNativePath(strPath);
}
#endif  // _WIN32
/**
 * @brief Convert a OS native path (wide string on Windows) to a generic UTF-8
 * path.
 * @details This is a helper, mainly for Windows and multi-platform projects.
 *
 * On Windows, backslash directory separators are converted to slash, and a
 * a UTF-8 string is returned, suitable for libraries that supports UTF-8 paths
 * like OpenSSL or drogon.
 *
 * On other OSes, returns an UTF-8 string without altering the directory
 * separators (backslashes are *NOT* replaced with slashes, since they
 * are valid characters for directory/file names).
 *
 * @param strPath Wide string or UTF-8 path.
 *
 * @return A generic path.
 */
inline const std::string &fromNativePath(const std::string &strPath)
{
    return trantor::utils::fromNativePath(strPath);
}
// Convert on all systems
inline std::string fromNativePath(const std::wstring &strPath)
{
    return trantor::utils::fromNativePath(strPath);
}

/// Replace all occurances of from to to inplace
/**
 * @param from string to replace
 * @param to string to replace with
 */
DROGON_EXPORT void replaceAll(std::string &s,
                              const std::string &from,
                              const std::string &to);

/**
 * @brief Generates cryptographically secure random bytes.
 *
 * @param ptr the pointer which the random bytes are stored to
 * @param size number of bytes to generate
 *
 * @return true if generation is successfull. False otherwise
 */
DROGON_EXPORT bool secureRandomBytes(void *ptr, size_t size);

/**
 * @brief Generates cryptographically secure random string.
 *
 * @param size number of characters to generate
 *
 * @return the random string
 */
DROGON_EXPORT std::string secureRandomString(size_t size);

template <typename T>
typename std::enable_if<internal::CanConvertFromStringStream<T>::value, T>::type
fromString(const std::string &p) noexcept(false)
{
    T value{};
    if (!p.empty())
    {
        std::stringstream ss(p);
        ss >> value;
    }
    return value;
}

template <typename T>
typename std::enable_if<!(internal::CanConvertFromStringStream<T>::value),
                        T>::type
fromString(const std::string &) noexcept(false)
{
    throw std::runtime_error("Bad type conversion");
}

template <>
inline std::string fromString<std::string>(const std::string &p) noexcept(false)
{
    return p;
}

template <>
inline int fromString<int>(const std::string &p) noexcept(false)
{
    return std::stoi(p);
}

template <>
inline long fromString<long>(const std::string &p) noexcept(false)
{
    return std::stol(p);
}

template <>
inline long long fromString<long long>(const std::string &p) noexcept(false)
{
    return std::stoll(p);
}

template <>
inline unsigned long fromString<unsigned long>(const std::string &p) noexcept(
    false)
{
    return std::stoul(p);
}

template <>
inline unsigned long long fromString<unsigned long long>(
    const std::string &p) noexcept(false)
{
    return std::stoull(p);
}

template <>
inline float fromString<float>(const std::string &p) noexcept(false)
{
    return std::stof(p);
}

template <>
inline double fromString<double>(const std::string &p) noexcept(false)
{
    return std::stod(p);
}

template <>
inline long double fromString<long double>(const std::string &p) noexcept(false)
{
    return std::stold(p);
}

template <>
inline bool fromString<bool>(const std::string &p) noexcept(false)
{
    if (p == "1")
    {
        return true;
    }
    if (p == "0")
    {
        return false;
    }
    std::string l{p};
    std::transform(p.begin(), p.end(), l.begin(), [](unsigned char c) {
        return (char)tolower(c);
    });
    if (l == "true")
    {
        return true;
    }
    else if (l == "false")
    {
        return false;
    }
    throw std::runtime_error("Can't convert from string '" + p + "' to bool");
}

DROGON_EXPORT bool supportsTls() noexcept;

namespace internal
{
DROGON_EXPORT extern const size_t fixedRandomNumber;
struct SafeStringHash
{
    size_t operator()(const std::string &str) const
    {
        const size_t A = 6665339;
        const size_t B = 2534641;
        size_t h = fixedRandomNumber;
        for (char ch : str)
            h = (h * A) ^ (ch * B);
        return h;
    }
};
}  // namespace internal
}  // namespace utils
}  // namespace drogon
