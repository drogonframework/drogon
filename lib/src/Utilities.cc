/**
 *
 *  @file Utilities.cc
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

#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/Utilities.h>
#include <drogon/config.h>
#ifdef USE_BROTLI
#include <brotli/decode.h>
#include <brotli/encode.h>
#endif
#ifdef _WIN32
#include <rpc.h>
#include <direct.h>
#include <io.h>
#include <iomanip>
#else
#include <uuid.h>
#include <unistd.h>
#endif
#include <zlib.h>
#include <sstream>
#include <string>
#include <mutex>
#include <random>
#include <algorithm>
#include <array>
#include <locale>
#include <clocale>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>

#ifdef _WIN32

char *strptime(const char *s, const char *f, struct tm *tm)
{
    // std::get_time is defined such that its
    // format parameters are the exact same as strptime.
    std::istringstream input(s);
    input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
    input >> std::get_time(tm, f);
    if (input.fail())
    {
        return nullptr;
    }
    return (char *)(s + input.tellg());
}

time_t timegm(struct tm *tm)
{
    struct tm my_tm;

    memcpy(&my_tm, tm, sizeof(struct tm));

    /* _mkgmtime() changes the value of the struct tm* you pass in, so
     * use a copy
     */
    return _mkgmtime(&my_tm);
}
#endif

#ifdef __HAIKU__
// HACK: Haiku has a timegm implementation. But it is not exposed
extern "C" time_t timegm(struct tm *tm);
#endif

namespace drogon
{
namespace utils
{
static constexpr std::string_view base64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static constexpr std::string_view urlBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

class Base64CharMap
{
  public:
    Base64CharMap()
    {
        char index = 0;
        for (int c = 'A'; c <= 'Z'; ++c)
        {
            charMap_[c] = index++;
        }
        for (int c = 'a'; c <= 'z'; ++c)
        {
            charMap_[c] = index++;
        }
        for (int c = '0'; c <= '9'; ++c)
        {
            charMap_[c] = index++;
        }
        charMap_[static_cast<int>('+')] = charMap_[static_cast<int>('-')] =
            index++;
        charMap_[static_cast<int>('/')] = charMap_[static_cast<int>('_')] =
            index;
        charMap_[0] = char(0xff);
    }

    char getIndex(const char c) const noexcept
    {
        return charMap_[static_cast<int>(c)];
    }

  private:
    char charMap_[256]{0};
};

static const Base64CharMap base64CharMap;

static inline bool isBase64(unsigned char c)
{
    if (isalnum(c))
        return true;
    switch (c)
    {
        case '+':
        case '/':
        case '-':
        case '_':
            return true;
    }
    return false;
}

bool isInteger(std::string_view str)
{
    for (auto c : str)
        if (c < '0' || c > '9')
            return false;
    return true;
}

bool isBase64(std::string_view str)
{
    for (auto c : str)
        if (!isBase64(c))
            return false;
    return true;
}

std::string genRandomString(int length)
{
    static const std::string_view char_space =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::uniform_int_distribution<size_t> dist(0, char_space.size() - 1);
    thread_local std::mt19937 rng(std::random_device{}());

    std::string str;
    str.resize(length);
    for (char &ch : str)
    {
        ch = char_space[dist(rng)];
    }

    return str;
}

std::vector<char> hexToBinaryVector(const char *ptr, size_t length)
{
    assert(length % 2 == 0);
    std::vector<char> ret(length / 2, '\0');
    for (size_t i = 0; i < ret.size(); ++i)
    {
        auto p = i * 2;
        char c1 = ptr[p];
        if (c1 >= '0' && c1 <= '9')
        {
            c1 -= '0';
        }
        else if (c1 >= 'a' && c1 <= 'f')
        {
            c1 -= 'a';
            c1 += 10;
        }
        else if (c1 >= 'A' && c1 <= 'F')
        {
            c1 -= 'A';
            c1 += 10;
        }
        else
        {
            return std::vector<char>();
        }
        char c2 = ptr[p + 1];
        if (c2 >= '0' && c2 <= '9')
        {
            c2 -= '0';
        }
        else if (c2 >= 'a' && c2 <= 'f')
        {
            c2 -= 'a';
            c2 += 10;
        }
        else if (c2 >= 'A' && c2 <= 'F')
        {
            c2 -= 'A';
            c2 += 10;
        }
        else
        {
            return std::vector<char>();
        }
        ret[i] = c1 * 16 + c2;
    }
    return ret;
}

std::string hexToBinaryString(const char *ptr, size_t length)
{
    assert(length % 2 == 0);
    std::string ret(length / 2, '\0');
    for (size_t i = 0; i < ret.length(); ++i)
    {
        auto p = i * 2;
        char c1 = ptr[p];
        if (c1 >= '0' && c1 <= '9')
        {
            c1 -= '0';
        }
        else if (c1 >= 'a' && c1 <= 'f')
        {
            c1 -= 'a';
            c1 += 10;
        }
        else if (c1 >= 'A' && c1 <= 'F')
        {
            c1 -= 'A';
            c1 += 10;
        }
        else
        {
            return "";
        }
        char c2 = ptr[p + 1];
        if (c2 >= '0' && c2 <= '9')
        {
            c2 -= '0';
        }
        else if (c2 >= 'a' && c2 <= 'f')
        {
            c2 -= 'a';
            c2 += 10;
        }
        else if (c2 >= 'A' && c2 <= 'F')
        {
            c2 -= 'A';
            c2 += 10;
        }
        else
        {
            return "";
        }
        ret[i] = c1 * 16 + c2;
    }
    return ret;
}

DROGON_EXPORT void binaryStringToHex(const char *ptr,
                                     size_t length,
                                     char *out,
                                     bool lowerCase)
{
    for (size_t i = 0; i < length; ++i)
    {
        int value = (ptr[i] & 0xf0) >> 4;
        if (value < 10)
        {
            out[i * 2] = char(value + 48);
        }
        else
        {
            if (!lowerCase)
            {
                out[i * 2] = char(value + 55);
            }
            else
            {
                out[i * 2] = char(value + 87);
            }
        }

        value = (ptr[i] & 0x0f);
        if (value < 10)
        {
            out[i * 2 + 1] = char(value + 48);
        }
        else
        {
            if (!lowerCase)
            {
                out[i * 2 + 1] = char(value + 55);
            }
            else
            {
                out[i * 2 + 1] = char(value + 87);
            }
        }
    }
}

std::string binaryStringToHex(const unsigned char *ptr,
                              size_t length,
                              bool lowercase)
{
    std::string idString(length * 2, '\0');
    binaryStringToHex((const char *)ptr, length, &idString[0], lowercase);
    return idString;
}

std::set<std::string> splitStringToSet(const std::string &str,
                                       const std::string &separator)
{
    std::set<std::string> ret;
    std::string::size_type pos1, pos2;
    pos2 = 0;
    pos1 = str.find(separator);
    while (pos1 != std::string::npos)
    {
        if (pos1 != 0)
        {
            std::string item = str.substr(pos2, pos1 - pos2);
            ret.insert(item);
        }
        pos2 = pos1 + separator.length();
        while (pos2 < str.length() &&
               str.substr(pos2, separator.length()) == separator)
            pos2 += separator.length();
        pos1 = str.find(separator, pos2);
    }
    if (pos2 < str.length())
        ret.insert(str.substr(pos2));
    return ret;
}

inline std::string createUuidString(const char *str, size_t len, bool lowercase)
{
    assert(len == 16);
    std::string uuid(36, '\0');
    binaryStringToHex(str, 4, &uuid[0], lowercase);
    uuid[8] = '-';
    binaryStringToHex(str + 4, 2, &uuid[9], lowercase);
    uuid[13] = '-';
    binaryStringToHex(str + 6, 2, &uuid[14], lowercase);
    uuid[18] = '-';
    binaryStringToHex(str + 8, 2, &uuid[19], lowercase);
    uuid[23] = '-';
    binaryStringToHex(str + 10, 6, &uuid[24], lowercase);
    return uuid;
}

std::string getUuid(bool lowercase)
{
#if USE_OSSP_UUID
    uuid_t *uuid;
    uuid_create(&uuid);
    uuid_make(uuid, UUID_MAKE_V4);
    char *str{nullptr};
    size_t len{0};
    uuid_export(uuid, UUID_FMT_BIN, &str, &len);
    uuid_destroy(uuid);
    auto ret = createUuidString(str, len, lowercase);
    free(str);
    return ret;
#elif defined __FreeBSD__ || defined __OpenBSD__
    uuid_t *uuid = new uuid_t;
    char *binstr = (char *)malloc(16);
#if defined __FreeBSD__
    uuidgen(uuid, 1);
#else
    uint32_t status;
    uuid_create(uuid, &status);
#endif
#if _BYTE_ORDER == _LITTLE_ENDIAN
    uuid_enc_le(binstr, uuid);
#else  /* _BYTE_ORDER != _LITTLE_ENDIAN */
    uuid_enc_be(binstr, uuid);
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */
    delete uuid;
    auto ret = createUuidString(binstr, 16, lowercase);
    free(binstr);
    return ret;
#elif defined _WIN32
    uuid_t uu;
    UuidCreate(&uu);
    char tempStr[100];
    auto len = snprintf(tempStr,
                        sizeof(tempStr),
                        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                        uu.Data1,
                        uu.Data2,
                        uu.Data3,
                        uu.Data4[0],
                        uu.Data4[1],
                        uu.Data4[2],
                        uu.Data4[3],
                        uu.Data4[4],
                        uu.Data4[5],
                        uu.Data4[6],
                        uu.Data4[7]);
    return std::string{tempStr, static_cast<size_t>(len)};
#else
    uuid_t uu;
    uuid_generate(uu);
    auto uuid = createUuidString((const char *)uu, 16, lowercase);
    return uuid;
#endif
}

void base64Encode(const unsigned char *bytesToEncode,
                  size_t inLen,
                  unsigned char *outputBuffer,
                  bool urlSafe,
                  bool padded)
{
    int i = 0;
    unsigned char charArray3[3];
    unsigned char charArray4[4];

    const std::string_view charSet = urlSafe ? urlBase64Chars : base64Chars;

    size_t a = 0;
    while (inLen--)
    {
        charArray3[i++] = *(bytesToEncode++);
        if (i == 3)
        {
            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] =
                ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] =
                ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;

            for (i = 0; (i < 4); ++i, ++a)
                outputBuffer[a] = charSet[charArray4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 3; ++j)
            charArray3[j] = '\0';

        charArray4[0] = (charArray3[0] & 0xfc) >> 2;
        charArray4[1] =
            ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
        charArray4[2] =
            ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
        charArray4[3] = charArray3[2] & 0x3f;

        for (int j = 0; (j <= i); ++j, ++a)
            outputBuffer[a] = charSet[charArray4[j]];

        if (padded)
            while ((++i < 4))
            {
                outputBuffer[a] = '=';
                ++a;
            }
    }
}

std::vector<char> base64DecodeToVector(std::string_view encodedString)
{
    auto inLen = encodedString.size();
    int i = 0;
    int in_{0};
    char charArray4[4], charArray3[3];
    std::vector<char> ret;
    ret.reserve(base64DecodedLength(inLen));

    while (inLen-- && (encodedString[in_] != '='))
    {
        if (!isBase64(encodedString[in_]))
        {
            ++in_;
            continue;
        }

        charArray4[i++] = encodedString[in_];
        ++in_;
        if (i == 4)
        {
            for (i = 0; i < 4; ++i)
            {
                charArray4[i] = base64CharMap.getIndex(charArray4[i]);
            }

            charArray3[0] =
                (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] =
                ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

            for (i = 0; (i < 3); ++i)
                ret.push_back(charArray3[i]);
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 4; ++j)
            charArray4[j] = 0;

        for (int j = 0; j < 4; ++j)
        {
            charArray4[j] = base64CharMap.getIndex(charArray4[j]);
        }

        charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
        charArray3[1] =
            ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
        charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

        --i;
        for (int j = 0; (j < i); ++j)
            ret.push_back(charArray3[j]);
    }

    return ret;
}

size_t base64Decode(const char *encodedString,
                    size_t inLen,
                    unsigned char *outputBuffer)
{
    int i = 0;
    int in_{0};
    unsigned char charArray4[4], charArray3[3];

    size_t a = 0;
    while (inLen-- && (encodedString[in_] != '='))
    {
        if (!isBase64(encodedString[in_]))
        {
            ++in_;
            continue;
        }

        charArray4[i++] = encodedString[in_];
        ++in_;
        if (i == 4)
        {
            for (i = 0; i < 4; ++i)
            {
                charArray4[i] = base64CharMap.getIndex(charArray4[i]);
            }
            charArray3[0] =
                (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] =
                ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

            for (i = 0; (i < 3); ++i, ++a)
                outputBuffer[a] = charArray3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 4; ++j)
            charArray4[j] = 0;

        for (int j = 0; j < 4; ++j)
        {
            charArray4[j] = base64CharMap.getIndex(charArray4[j]);
        }

        charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
        charArray3[1] =
            ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
        charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

        --i;
        for (int j = 0; (j < i); ++j, ++a)
            outputBuffer[a] = charArray3[j];
    }

    return a;
}

static std::string charToHex(char c)
{
    std::string result;
    char first, second;

    first = (c & 0xF0) / 16;
    first += first > 9 ? 'A' - 10 : '0';
    second = c & 0x0F;
    second += second > 9 ? 'A' - 10 : '0';

    result.append(1, first);
    result.append(1, second);

    return result;
}

std::string urlEncodeComponent(const std::string &src)
{
    std::string result;
    std::string::const_iterator iter;

    for (iter = src.begin(); iter != src.end(); ++iter)
    {
        switch (*iter)
        {
            case ' ':
                result.append(1, '+');
                break;
            // alnum
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            // mark
            case '-':
            case '_':
            case '.':
            case '!':
            case '~':
            case '*':
            case '(':
            case ')':
                result.append(1, *iter);
                break;
            // escape
            default:
                result.append(1, '%');
                result.append(charToHex(*iter));
                break;
        }
    }

    return result;
}

std::string urlEncode(const std::string &src)
{
    std::string result;
    std::string::const_iterator iter;

    for (iter = src.begin(); iter != src.end(); ++iter)
    {
        switch (*iter)
        {
            case ' ':
                result.append(1, '+');
                break;
            // alnum
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            // mark
            case '-':
            case '_':
            case '.':
            case '!':
            case '~':
            case '*':
            case '\'':
            case '(':
            case ')':
            case '&':
            case '=':
            case '/':
            case '\\':
            case '?':
                result.append(1, *iter);
                break;
            // escape
            default:
                result.append(1, '%');
                result.append(charToHex(*iter));
                break;
        }
    }

    return result;
}

bool needUrlDecoding(const char *begin, const char *end)
{
    return std::find_if(begin, end, [](const char c) {
               return c == '+' || c == '%';
           }) != end;
}

std::string urlDecode(const char *begin, const char *end)
{
    std::string result;
    size_t len = end - begin;
    result.reserve(len * 2);
    int hex = 0;
    for (size_t i = 0; i < len; ++i)
    {
        switch (begin[i])
        {
            case '+':
                result += ' ';
                break;
            case '%':
                if ((i + 2) < len && isxdigit(begin[i + 1]) &&
                    isxdigit(begin[i + 2]))
                {
                    unsigned int x1 = begin[i + 1];
                    if (x1 >= '0' && x1 <= '9')
                    {
                        x1 -= '0';
                    }
                    else if (x1 >= 'a' && x1 <= 'f')
                    {
                        x1 = x1 - 'a' + 10;
                    }
                    else if (x1 >= 'A' && x1 <= 'F')
                    {
                        x1 = x1 - 'A' + 10;
                    }
                    unsigned int x2 = begin[i + 2];
                    if (x2 >= '0' && x2 <= '9')
                    {
                        x2 -= '0';
                    }
                    else if (x2 >= 'a' && x2 <= 'f')
                    {
                        x2 = x2 - 'a' + 10;
                    }
                    else if (x2 >= 'A' && x2 <= 'F')
                    {
                        x2 = x2 - 'A' + 10;
                    }
                    hex = x1 * 16 + x2;

                    result += char(hex);
                    i += 2;
                }
                else
                {
                    result += '%';
                }
                break;
            default:
                result += begin[i];
                break;
        }
    }
    return result;
}

/* Compress gzip data */
std::string gzipCompress(const char *data, const size_t ndata)
{
    z_stream strm = {nullptr,
                     0,
                     0,
                     nullptr,
                     0,
                     0,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     0,
                     0,
                     0};
    if (data && ndata > 0)
    {
        if (deflateInit2(&strm,
                         Z_DEFAULT_COMPRESSION,
                         Z_DEFLATED,
                         MAX_WBITS + 16,
                         8,
                         Z_DEFAULT_STRATEGY) != Z_OK)
        {
            LOG_ERROR << "deflateInit2 error!";
            return std::string{};
        }
        std::string outstr;
        outstr.resize(compressBound(static_cast<uLong>(ndata)));
        strm.next_in = (Bytef *)data;
        strm.avail_in = static_cast<uInt>(ndata);
        int ret;
        do
        {
            if (strm.total_out >= outstr.size())
            {
                outstr.resize(strm.total_out * 2);
            }
            assert(outstr.size() >= strm.total_out);
            strm.avail_out = static_cast<uInt>(outstr.size() - strm.total_out);
            strm.next_out = (Bytef *)outstr.data() + strm.total_out;
            ret = deflate(&strm, Z_FINISH); /* no bad return value */
            if (ret == Z_STREAM_ERROR)
            {
                (void)deflateEnd(&strm);
                return std::string{};
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);
        assert(ret == Z_STREAM_END); /* stream will be complete */
        outstr.resize(strm.total_out);
        /* clean up and return */
        (void)deflateEnd(&strm);
        return outstr;
    }
    return std::string{};
}

std::string gzipDecompress(const char *data, const size_t ndata)
{
    if (ndata == 0)
        return std::string(data, ndata);

    auto full_length = ndata;

    auto decompressed = std::string(full_length * 2, 0);
    bool done = false;

    z_stream strm = {nullptr,
                     0,
                     0,
                     nullptr,
                     0,
                     0,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     0,
                     0,
                     0};
    strm.next_in = (Bytef *)data;
    strm.avail_in = static_cast<uInt>(ndata);
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    if (inflateInit2(&strm, (15 + 32)) != Z_OK)
    {
        LOG_ERROR << "inflateInit2 error!";
        return std::string{};
    }
    while (!done)
    {
        // Make sure we have enough room and reset the lengths.
        if (strm.total_out >= decompressed.length())
        {
            decompressed.resize(decompressed.length() * 2);
        }
        strm.next_out = (Bytef *)decompressed.data() + strm.total_out;
        strm.avail_out =
            static_cast<uInt>(decompressed.length() - strm.total_out);
        // Inflate another chunk.
        int status = inflate(&strm, Z_SYNC_FLUSH);
        if (status == Z_STREAM_END)
        {
            done = true;
        }
        else if (status != Z_OK)
        {
            break;
        }
    }
    if (inflateEnd(&strm) != Z_OK)
        return std::string{};
    // Set real length.
    if (done)
    {
        decompressed.resize(strm.total_out);
        return decompressed;
    }
    else
    {
        return std::string{};
    }
}

char *getHttpFullDate(const trantor::Date &date)
{
    static thread_local int64_t lastSecond = 0;
    static thread_local char lastTimeString[128] = {0};
    auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
    if (nowSecond == lastSecond)
    {
        return lastTimeString;
    }
    lastSecond = nowSecond;
    date.toCustomFormattedString("%a, %d %b %Y %H:%M:%S GMT",
                                 lastTimeString,
                                 sizeof(lastTimeString));
    return lastTimeString;
}

void dateToCustomFormattedString(const std::string &fmtStr,
                                 std::string &str,
                                 const trantor::Date &date)
{
    auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
    time_t seconds = static_cast<time_t>(nowSecond);
    struct tm tm_LValue = date.tmStruct();
    std::stringstream Out;
    Out.imbue(std::locale{"C"});
    Out << std::put_time(&tm_LValue, fmtStr.c_str());
    str = Out.str();
}

const std::string &getHttpFullDateStr(const trantor::Date &date)
{
    static thread_local int64_t lastSecond = 0;
    static thread_local std::string lastTimeString(128, 0);
    auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
    if (nowSecond == lastSecond)
    {
        return lastTimeString;
    }
    lastSecond = nowSecond;
    dateToCustomFormattedString("%a, %d %b %Y %H:%M:%S GMT",
                                lastTimeString,
                                date);
    return lastTimeString;
}

trantor::Date getHttpDate(const std::string &httpFullDateString)
{
    static const std::array<const char *, 4> formats = {
        // RFC822 (default)
        "%a, %d %b %Y %H:%M:%S",
        // RFC 850 (deprecated)
        "%a, %d-%b-%y %H:%M:%S",
        // ansi asctime format
        "%a %b %d %H:%M:%S %Y",
        // weird RFC 850-hybrid thing that reddit uses
        "%a, %d-%b-%Y %H:%M:%S",
    };
    struct tm tmptm;
    for (const char *format : formats)
    {
        if (strptime(httpFullDateString.c_str(), format, &tmptm) != NULL)
        {
            auto epoch = timegm(&tmptm);
            return trantor::Date(epoch * MICRO_SECONDS_PRE_SEC);
        }
    }
    LOG_WARN << "invalid datetime format: '" << httpFullDateString << "'";
    return trantor::Date((std::numeric_limits<int64_t>::max)());
}

std::string formattedString(const char *format, ...)
{
    std::string strBuffer(128, 0);
    va_list ap, backup_ap;
    va_start(ap, format);
    va_copy(backup_ap, ap);
    auto result = vsnprintf((char *)strBuffer.data(),
                            strBuffer.size(),
                            format,
                            backup_ap);
    va_end(backup_ap);
    if ((result >= 0) && ((std::string::size_type)result < strBuffer.size()))
    {
        strBuffer.resize(result);
    }
    else
    {
        while (true)
        {
            if (result < 0)
            {
                // Older snprintf() behavior. Just try doubling the buffer size
                strBuffer.resize(strBuffer.size() * 2);
            }
            else
            {
                strBuffer.resize(result + 1);
            }

            va_copy(backup_ap, ap);
            auto result = vsnprintf((char *)strBuffer.data(),
                                    strBuffer.size(),
                                    format,
                                    backup_ap);
            va_end(backup_ap);

            if ((result >= 0) &&
                ((std::string::size_type)result < strBuffer.size()))
            {
                strBuffer.resize(result);
                break;
            }
        }
    }
    va_end(ap);
    return strBuffer;
}

int createPath(const std::string &path)
{
    if (path.empty())
        return 0;
    auto osPath{toNativePath(path)};
    if (osPath.back() != std::filesystem::path::preferred_separator)
        osPath.push_back(std::filesystem::path::preferred_separator);
    std::filesystem::path fsPath(osPath);
    std::error_code err;
    std::filesystem::create_directories(fsPath, err);
    if (err)
    {
        LOG_ERROR << "Error " << err.value() << " creating path " << osPath
                  << ": " << err.message();
        return -1;
    }
    return 0;
}
#ifdef USE_BROTLI
std::string brotliCompress(const char *data, const size_t ndata)
{
    std::string ret;
    if (ndata == 0)
        return ret;
    ret.resize(BrotliEncoderMaxCompressedSize(ndata));
    size_t encodedSize{ret.size()};
    auto r = BrotliEncoderCompress(5,
                                   BROTLI_DEFAULT_WINDOW,
                                   BROTLI_DEFAULT_MODE,
                                   ndata,
                                   (const uint8_t *)(data),
                                   &encodedSize,
                                   (uint8_t *)(ret.data()));
    if (r == BROTLI_FALSE)
        ret.resize(0);
    else
        ret.resize(encodedSize);
    return ret;
}

std::string brotliDecompress(const char *data, const size_t ndata)
{
    if (ndata == 0)
        return std::string(data, ndata);

    size_t availableIn = ndata;
    auto nextIn = (const uint8_t *)(data);
    auto decompressed = std::string(availableIn * 3, 0);
    size_t availableOut = decompressed.size();
    auto nextOut = (uint8_t *)(decompressed.data());
    size_t totalOut{0};
    bool done = false;
    auto s = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    while (!done)
    {
        auto result = BrotliDecoderDecompressStream(
            s, &availableIn, &nextIn, &availableOut, &nextOut, &totalOut);
        if (result == BROTLI_DECODER_RESULT_SUCCESS)
        {
            decompressed.resize(totalOut);
            done = true;
        }
        else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
        {
            assert(totalOut == decompressed.size());
            decompressed.resize(totalOut * 2);
            nextOut = (uint8_t *)(decompressed.data() + totalOut);
            availableOut = totalOut;
        }
        else
        {
            decompressed.resize(0);
            done = true;
        }
    }
    BrotliDecoderDestroyInstance(s);
    return decompressed;
}
#else
std::string brotliCompress(const char * /*data*/, const size_t /*ndata*/)
{
    LOG_ERROR << "If you do not have the brotli package installed, you cannot "
                 "use brotliCompress()";
    abort();
}

std::string brotliDecompress(const char * /*data*/, const size_t /*ndata*/)
{
    LOG_ERROR << "If you do not have the brotli package installed, you cannot "
                 "use brotliDecompress()";
    abort();
}
#endif

std::string getMd5(const char *data, const size_t dataLen)
{
    return trantor::utils::toHexString(trantor::utils::md5(data, dataLen));
}

std::string getSha1(const char *data, const size_t dataLen)
{
    return trantor::utils::toHexString(trantor::utils::sha1(data, dataLen));
}

std::string getSha256(const char *data, const size_t dataLen)
{
    return trantor::utils::toHexString(trantor::utils::sha256(data, dataLen));
}

std::string getSha3(const char *data, const size_t dataLen)
{
    return trantor::utils::toHexString(trantor::utils::sha3(data, dataLen));
}

std::string getBlake2b(const char *data, const size_t dataLen)
{
    return trantor::utils::toHexString(trantor::utils::blake2b(data, dataLen));
}

void replaceAll(std::string &s, const std::string &from, const std::string &to)
{
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool supportsTls() noexcept
{
    return trantor::utils::tlsBackend() != "None";
}

bool secureRandomBytes(void *ptr, size_t size)
{
    return trantor::utils::secureRandomBytes(ptr, size);
}

std::string secureRandomString(size_t size)
{
    if (size == 0)
        return std::string();

    std::string ret(size, 0);
    const std::string_view chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "+-";
    assert(chars.size() == 64);

    // batch up to 32 bytes of random data for efficiency. Calling
    // secureRandomBytes can be expensive.
    auto randByte = []() {
        thread_local trantor::utils::Hash256 hash;
        thread_local size_t i = 0;
        if (i == 0)
        {
            bool ok = trantor::utils::secureRandomBytes(&hash, sizeof(hash));
            if (!ok)
                throw std::runtime_error(
                    "Failed to generate random bytes for secureRandomString");
        }
        unsigned char *hashBytes = reinterpret_cast<unsigned char *>(&hash);
        auto ret = hashBytes[i];
        i = (i + 1) % sizeof(hash);
        return ret;
    };

    for (size_t i = 0; i < size; ++i)
        ret[i] = chars[randByte() % 64];
    return ret;
}

namespace internal
{
const size_t fixedRandomNumber = []() {
    size_t res;
    utils::secureRandomBytes(&res, sizeof(res));
    return res;
}();
}

}  // namespace utils
}  // namespace drogon
