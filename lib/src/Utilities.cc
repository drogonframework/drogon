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
#include <drogon/config.h>
#include <trantor/utils/Logger.h>
#include <string.h>
#include <zlib.h>
#include <uuid.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <stack>

namespace drogon
{
static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

bool isInteger(const std::string &str)
{
    for (auto c : str)
    {
        if (c > '9' || c < '0')
            return false;
    }
    return true;
}

std::string genRandomString(int length)
{
    static const char char_space[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::once_flag once;
    static const int len = strlen(char_space);
    static const int randMax = RAND_MAX - (RAND_MAX % len);
    std::call_once(once, []() {
        std::srand(time(nullptr));
    });

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
std::vector<std::string> splitString(const std::string &str, const std::string &separator)
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
        while (pos2 < str.length() && str.substr(pos2, separator.length()) == separator)
            pos2 += separator.length();
        pos1 = str.find(separator, pos2);
    }
    if (pos2 < str.length())
        ret.push_back(str.substr(pos2));
    return ret;
}
std::string getuuid()
{
    uuid_t uu;
    uuid_generate(uu);
    return binaryStringToHex(uu, 16);
}

std::string base64Encode(const unsigned char *bytes_to_encode, unsigned int in_len)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::string base64Decode(std::string const &encoded_string)
{
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++)
            ret += char_array_3[j];
    }

    return ret;
}

std::string urlDecode(const std::string &szToDecode)
{
    return urlDecode(szToDecode.c_str(), szToDecode.c_str() + szToDecode.length());
}

std::string urlDecode(const char *begin, const char *end)
{
    std::string result;
    size_t len = end - begin;
    result.reserve(len);
    int hex = 0;
    for (size_t i = 0; i < len; ++i)
    {
        switch (begin[i])
        {
        case '+':
            result += ' ';
            break;
        case '%':
            if ((i + 2) < len && isxdigit(begin[i + 1]) && isxdigit(begin[i + 2]))
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
                //字母和数字[0-9a-zA-Z]、一些特殊符号[$-_.+!*'(),] 、以及某些保留字[$&+,/:;=?@]
                //可以不经过编码直接用于URL
                if (!((hex >= 48 && hex <= 57) ||  //0-9
                      (hex >= 97 && hex <= 122) || //a-z
                      (hex >= 65 && hex <= 90) ||  //A-Z
                      //一些特殊符号及保留字[$-_.+!*'(),]  [$&+,/:;=?@]
                      hex == 0x21 || hex == 0x24 || hex == 0x26 || hex == 0x27 || hex == 0x28 || hex == 0x29 || hex == 0x2a || hex == 0x2b || hex == 0x2c || hex == 0x2d || hex == 0x2e || hex == 0x2f || hex == 0x3A || hex == 0x3B || hex == 0x3D || hex == 0x3f || hex == 0x40 || hex == 0x5f))
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
/* data 原数据 ndata 原数据长度 zdata 压缩后数据 nzdata 压缩后长度 */

int gzipCompress(const char *data, const size_t ndata,
                 char *zdata, size_t *nzdata)
{

    z_stream c_stream;
    int err = 0;

    if (data && ndata > 0)
    {
        c_stream.zalloc = NULL;
        c_stream.zfree = NULL;
        c_stream.opaque = NULL;
        //只有设置为MAX_WBITS + 16才能在在压缩文本中带header和trailer
        if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return -1;
        c_stream.next_in = (Bytef *)data;
        c_stream.avail_in = ndata;
        c_stream.next_out = (Bytef *)zdata;
        c_stream.avail_out = *nzdata;
        while (c_stream.avail_in != 0 && c_stream.total_out < *nzdata)
        {
            if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
            {
                LOG_ERROR << "compress err:" << c_stream.msg;
                return -1;
            }
        }
        if (c_stream.avail_in != 0)
            return c_stream.avail_in;
        for (;;)
        {
            if ((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END)
                break;
            if (err != Z_OK)
                return -1;
        }
        if (deflateEnd(&c_stream) != Z_OK)
            return -1;
        *nzdata = c_stream.total_out;
        return 0;
    }
    return -1;
}

/* Uncompress gzip data */
/* zdata 数据 nzdata 原数据长度 data 解压后数据 ndata 解压后长度 */
int gzipDecompress(const char *zdata, const size_t nzdata,
                   char *data, size_t *ndata)
{
    int err = 0;
    z_stream d_stream = {0}; /* decompression stream */
    static char dummy_head[2] = {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };
    d_stream.zalloc = NULL;
    d_stream.zfree = NULL;
    d_stream.opaque = NULL;
    d_stream.next_in = (Bytef *)zdata;
    d_stream.avail_in = 0;
    d_stream.next_out = (Bytef *)data;
    //只有设置为MAX_WBITS + 16才能在解压带header和trailer的文本
    if (inflateInit2(&d_stream, MAX_WBITS + 16) != Z_OK)
        return -1;
    //if(inflateInit2(&d_stream, 47) != Z_OK) return -1;
    while (d_stream.total_out < *ndata && d_stream.total_in < nzdata)
    {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        if ((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END)
            break;
        if (err != Z_OK)
        {
            if (err == Z_DATA_ERROR)
            {
                d_stream.next_in = (Bytef *)dummy_head;
                d_stream.avail_in = sizeof(dummy_head);
                if ((err = inflate(&d_stream, Z_NO_FLUSH)) != Z_OK)
                {
                    return -1;
                }
            }
            else
                return -1;
        }
    }
    if (inflateEnd(&d_stream) != Z_OK)
        return -1;
    *ndata = d_stream.total_out;
    return 0;
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
    date.toCustomedFormattedString("%a, %d %b %Y %T GMT", lastTimeString, sizeof(lastTimeString));
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
    std::string strBuffer;
    strBuffer.resize(1024);
    va_list ap, backup_ap;
    va_start(ap, format);
    va_copy(backup_ap, ap);
    auto result = vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
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
            auto result = vsnprintf((char *)strBuffer.data(), strBuffer.size(), format, backup_ap);
            va_end(backup_ap);

            if ((result >= 0) && ((std::string::size_type)result < strBuffer.size()))
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

} // namespace drogon
