/**
 *
 *  Utilities.cc
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

#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <uuid.h>
#include <zlib.h>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stack>
#include <string>
#include <thread>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

namespace drogon
{
namespace utils
{
static const std::string base64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool isBase64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

bool isInteger(const std::string &str)
{
    for (auto const &c : str)
    {
        if (c > '9' || c < '0')
            return false;
    }
    return true;
}

std::string genRandomString(int length)
{
    static const char char_space[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::once_flag once;
    static const int len = strlen(char_space);
    static const int randMax = RAND_MAX - (RAND_MAX % len);
    std::call_once(once, []() { std::srand(time(nullptr)); });

    int i;
    std::string str;
    str.resize(length);

    for (i = 0; i < length; i++)
    {
        int x = std::rand();
        while (x >= randMax)
        {
            x = std::rand();
        }
        x = (x % len);
        str[i] = char_space[x];
    }

    return str;
}

std::vector<char> hexToBinaryVector(const char *ptr, size_t length)
{
    assert(length % 2 == 0);
    std::vector<char> ret(length / 2, '\0');
    for (size_t i = 0; i < ret.size(); i++)
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
    for (size_t i = 0; i < ret.length(); i++)
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
std::string binaryStringToHex(const unsigned char *ptr, size_t length)
{
    std::string idString;
    for (size_t i = 0; i < length; i++)
    {
        int value = (ptr[i] & 0xf0) >> 4;
        if (value < 10)
        {
            idString.append(1, char(value + 48));
        }
        else
        {
            idString.append(1, char(value + 55));
        }

        value = (ptr[i] & 0x0f);
        if (value < 10)
        {
            idString.append(1, char(value + 48));
        }
        else
        {
            idString.append(1, char(value + 55));
        }
    }
    return idString;
}
std::vector<std::string> splitString(const std::string &str,
                                     const std::string &separator)
{
    std::vector<std::string> ret;
    std::string::size_type pos1, pos2;
    pos2 = 0;
    pos1 = str.find(separator);
    while (pos1 != std::string::npos)
    {
        if (pos1 != 0)
        {
            std::string item = str.substr(pos2, pos1 - pos2);
            ret.push_back(item);
        }
        pos2 = pos1 + separator.length();
        while (pos2 < str.length() &&
               str.substr(pos2, separator.length()) == separator)
            pos2 += separator.length();
        pos1 = str.find(separator, pos2);
    }
    if (pos2 < str.length())
        ret.push_back(str.substr(pos2));
    return ret;
}
std::string getUuid()
{
    uuid_t uu;
    uuid_generate(uu);
    return binaryStringToHex(uu, 16);
}

std::string base64Encode(const unsigned char *bytes_to_encode,
                         unsigned int in_len)
{
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64Chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
            ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] =
            ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; (j < i + 1); j++)
            ret += base64Chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::string base64Decode(std::string const &encoded_string)
{
    int in_len = encoded_string.size();
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') &&
           isBase64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64Chars.find(char_array_4[i]);

            char_array_3[0] =
                (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
                              ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (int j = 0; j < 4; j++)
            char_array_4[j] = base64Chars.find(char_array_4[j]);

        char_array_3[0] =
            (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] =
            ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int j = 0; (j < i - 1); j++)
            ret += char_array_3[j];
    }

    return ret;
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
                    uint x1 = begin[i + 1];
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
                    uint x2 = begin[i + 2];
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
                    if (!((hex >= 48 && hex <= 57) ||   // 0-9
                          (hex >= 97 && hex <= 122) ||  // a-z
                          (hex >= 65 && hex <= 90) ||   // A-Z
                          //[$-_.+!*'(),]  [$&+,/:;?@]
                          hex == 0x21 || hex == 0x24 || hex == 0x26 ||
                          hex == 0x27 || hex == 0x28 || hex == 0x29 ||
                          hex == 0x2a || hex == 0x2b || hex == 0x2c ||
                          hex == 0x2d || hex == 0x2e || hex == 0x2f ||
                          hex == 0x3A || hex == 0x3B || hex == 0x3f ||
                          hex == 0x40 || hex == 0x5f))
                    {
                        result += char(hex);
                        i += 2;
                    }
                    else
                        result += '%';
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
    z_stream strm = {0};
    if (data && ndata > 0)
    {
        if (deflateInit2(&strm,
                         Z_DEFAULT_COMPRESSION,
                         Z_DEFLATED,
                         MAX_WBITS + 16,
                         8,
                         Z_DEFAULT_STRATEGY) != Z_OK)
            return nullptr;
        std::string outstr;
        outstr.resize(compressBound(ndata));
        strm.next_in = (Bytef *)data;
        strm.avail_in = ndata;
        strm.next_out = (Bytef *)outstr.data();
        strm.avail_out = outstr.length();
        if (deflate(&strm, Z_FINISH) != Z_STREAM_END)
            return nullptr;
        if (deflateEnd(&strm) != Z_OK)
            return nullptr;
        outstr.resize(strm.total_out);
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

    z_stream strm = {0};
    strm.next_in = (Bytef *)data;
    strm.avail_in = ndata;
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    if (inflateInit2(&strm, (15 + 32)) != Z_OK)
        return nullptr;
    while (!done)
    {
        // Make sure we have enough room and reset the lengths.
        if (strm.total_out >= decompressed.length())
        {
            decompressed.resize(decompressed.length() * 2);
        }
        strm.next_out = (Bytef *)decompressed.data() + strm.total_out;
        strm.avail_out = decompressed.length() - strm.total_out;
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
    static __thread int64_t lastSecond = 0;
    static __thread char lastTimeString[128] = {0};
    auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
    if (nowSecond == lastSecond)
    {
        return lastTimeString;
    }
    lastSecond = nowSecond;
    date.toCustomedFormattedString("%a, %d %b %Y %T GMT",
                                   lastTimeString,
                                   sizeof(lastTimeString));
    return lastTimeString;
}
trantor::Date getHttpDate(const std::string &httpFullDateString)
{
    struct tm tmptm;
    strptime(httpFullDateString.c_str(), "%a, %d %b %Y %T", &tmptm);
    auto epoch = timegm(&tmptm);
    return trantor::Date(epoch * MICRO_SECONDS_PRE_SEC);
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
    auto tmpPath = path;
    std::stack<std::string> pathStack;
    while (access(tmpPath.c_str(), F_OK) != 0)
    {
        if (tmpPath == "./" || tmpPath == "/")
            return -1;
        while (tmpPath[tmpPath.length() - 1] == '/')
            tmpPath.resize(tmpPath.length() - 1);
        auto pos = tmpPath.rfind("/");
        if (pos != std::string::npos)
        {
            pathStack.push(tmpPath.substr(pos));
            tmpPath = tmpPath.substr(0, pos + 1);
        }
        else
        {
            pathStack.push(tmpPath);
            tmpPath.clear();
            break;
        }
    }
    while (pathStack.size() > 0)
    {
        if (tmpPath.empty())
        {
            tmpPath = pathStack.top();
        }
        else
        {
            if (tmpPath[tmpPath.length() - 1] == '/')
            {
                tmpPath.append(pathStack.top());
            }
            else
            {
                tmpPath.append("/").append(pathStack.top());
            }
        }
        pathStack.pop();

        if (mkdir(tmpPath.c_str(), 0755) == -1)
        {
            LOG_ERROR << "Can't create path:" << path;
            return -1;
        }
    }
    return 0;
}

}  // namespace utils
}  // namespace drogon
