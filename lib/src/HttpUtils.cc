/**
 *
 *  @file HttpUtils.cc
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

#include "HttpUtils.h"
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <unordered_map>

namespace drogon
{
static std::unordered_map<std::string, std::string> customMime;

const std::string_view &statusCodeToString(int code)
{
    switch (code)
    {
        case 100:
        {
            static std::string_view sv = "Continue";
            return sv;
        }
        case 101:
        {
            static std::string_view sv = "Switching Protocols";
            return sv;
        }
        case 102:
        {
            static std::string_view sv = "Processing";
            return sv;
        }
        case 103:
        {
            static std::string_view sv = "Early Hints";
            return sv;
        }
        case 200:
        {
            static std::string_view sv = "OK";
            return sv;
        }
        case 201:
        {
            static std::string_view sv = "Created";
            return sv;
        }
        case 202:
        {
            static std::string_view sv = "Accepted";
            return sv;
        }
        case 203:
        {
            static std::string_view sv = "Non-Authoritative Information";
            return sv;
        }
        case 204:
        {
            static std::string_view sv = "No Content";
            return sv;
        }
        case 205:
        {
            static std::string_view sv = "Reset Content";
            return sv;
        }
        case 206:
        {
            static std::string_view sv = "Partial Content";
            return sv;
        }
        case 207:
        {
            static std::string_view sv = "Multi-Status";
            return sv;
        }
        case 208:
        {
            static std::string_view sv = "Already Reported";
            return sv;
        }
        case 226:
        {
            static std::string_view sv = "IM Used";
            return sv;
        }
        case 300:
        {
            static std::string_view sv = "Multiple Choices";
            return sv;
        }
        case 301:
        {
            static std::string_view sv = "Moved Permanently";
            return sv;
        }
        case 302:
        {
            static std::string_view sv = "Found";
            return sv;
        }
        case 303:
        {
            static std::string_view sv = "See Other";
            return sv;
        }
        case 304:
        {
            static std::string_view sv = "Not Modified";
            return sv;
        }
        case 305:
        {
            static std::string_view sv = "Use Proxy";
            return sv;
        }
        case 306:
        {
            static std::string_view sv = "(Unused)";
            return sv;
        }
        case 307:
        {
            static std::string_view sv = "Temporary Redirect";
            return sv;
        }
        case 308:
        {
            static std::string_view sv = "Permanent Redirect";
            return sv;
        }
        case 400:
        {
            static std::string_view sv = "Bad Request";
            return sv;
        }
        case 401:
        {
            static std::string_view sv = "Unauthorized";
            return sv;
        }
        case 402:
        {
            static std::string_view sv = "Payment Required";
            return sv;
        }
        case 403:
        {
            static std::string_view sv = "Forbidden";
            return sv;
        }
        case 404:
        {
            static std::string_view sv = "Not Found";
            return sv;
        }
        case 405:
        {
            static std::string_view sv = "Method Not Allowed";
            return sv;
        }
        case 406:
        {
            static std::string_view sv = "Not Acceptable";
            return sv;
        }
        case 407:
        {
            static std::string_view sv = "Proxy Authentication Required";
            return sv;
        }
        case 408:
        {
            static std::string_view sv = "Request Time-out";
            return sv;
        }
        case 409:
        {
            static std::string_view sv = "Conflict";
            return sv;
        }
        case 410:
        {
            static std::string_view sv = "Gone";
            return sv;
        }
        case 411:
        {
            static std::string_view sv = "Length Required";
            return sv;
        }
        case 412:
        {
            static std::string_view sv = "Precondition Failed";
            return sv;
        }
        case 413:
        {
            static std::string_view sv = "Request Entity Too Large";
            return sv;
        }
        case 414:
        {
            static std::string_view sv = "Request-URI Too Large";
            return sv;
        }
        case 415:
        {
            static std::string_view sv = "Unsupported Media Type";
            return sv;
        }
        case 416:
        {
            static std::string_view sv = "Requested Range Not Satisfiable";
            return sv;
        }
        case 417:
        {
            static std::string_view sv = "Expectation Failed";
            return sv;
        }
        case 418:
        {
            static std::string_view sv = "I'm a Teapot";
            return sv;
        }
        case 421:
        {
            static std::string_view sv = "Misdirected Request";
            return sv;
        }
        case 422:
        {
            static std::string_view sv = "Unprocessable Entity";
            return sv;
        }
        case 423:
        {
            static std::string_view sv = "Locked";
            return sv;
        }
        case 424:
        {
            static std::string_view sv = "Failed Dependency";
            return sv;
        }
        case 425:
        {
            static std::string_view sv = "Too Early";
            return sv;
        }
        case 426:
        {
            static std::string_view sv = "Upgrade Required";
            return sv;
        }
        case 428:
        {
            static std::string_view sv = "Precondition Required";
            return sv;
        }
        case 429:
        {
            static std::string_view sv = "Too Many Requests";
            return sv;
        }
        case 431:
        {
            static std::string_view sv = "Request Header Fields Too Large";
            return sv;
        }
        case 451:
        {
            static std::string_view sv = "Unavailable For Legal Reasons";
            return sv;
        }
        case 500:
        {
            static std::string_view sv = "Internal Server Error";
            return sv;
        }
        case 501:
        {
            static std::string_view sv = "Not Implemented";
            return sv;
        }
        case 502:
        {
            static std::string_view sv = "Bad Gateway";
            return sv;
        }
        case 503:
        {
            static std::string_view sv = "Service Unavailable";
            return sv;
        }
        case 504:
        {
            static std::string_view sv = "Gateway Time-out";
            return sv;
        }
        case 505:
        {
            static std::string_view sv = "HTTP Version Not Supported";
            return sv;
        }
        case 506:
        {
            static std::string_view sv = "Variant Also Negotiates";
            return sv;
        }
        case 507:
        {
            static std::string_view sv = "Insufficient Storage";
            return sv;
        }
        case 508:
        {
            static std::string_view sv = "Loop Detected";
            return sv;
        }
        case 510:
        {
            static std::string_view sv = "Not Extended";
            return sv;
        }
        case 511:
        {
            static std::string_view sv = "Network Authentication Required";
            return sv;
        }
        default:
            if (code >= 100 && code < 200)
            {
                static std::string_view sv = "Informational";
                return sv;
            }
            else if (code >= 200 && code < 300)
            {
                static std::string_view sv = "Successful";
                return sv;
            }
            else if (code >= 300 && code < 400)
            {
                static std::string_view sv = "Redirection";
                return sv;
            }
            else if (code >= 400 && code < 500)
            {
                static std::string_view sv = "Bad Request";
                return sv;
            }
            else if (code >= 500 && code < 600)
            {
                static std::string_view sv = "Server Error";
                return sv;
            }
            else
            {
                static std::string_view sv = "Undefined Error";
                return sv;
            }
    }
}

ContentType getContentType(const std::string &fileName)
{
    std::string extName;
    auto pos = fileName.rfind('.');
    if (pos != std::string::npos)
    {
        extName = fileName.substr(pos + 1);
        transform(extName.begin(),
                  extName.end(),
                  extName.begin(),
                  [](unsigned char c) { return tolower(c); });
    }
    switch (extName.length())
    {
        case 0:
            return CT_APPLICATION_OCTET_STREAM;
        case 2:
        {
            if (extName == "js")
                return CT_TEXT_JAVASCRIPT;
            return CT_APPLICATION_OCTET_STREAM;
        }
        case 3:
        {
            switch (extName[0])
            {
                case 'b':
                    if (extName == "bmp")
                        return CT_IMAGE_BMP;
                    break;
                case 'c':
                    if (extName == "css")
                        return CT_TEXT_CSS;
                    break;
                case 'e':
                    if (extName == "eot")
                        return CT_APPLICATION_VND_MS_FONTOBJ;
                    break;
                case 'g':
                    if (extName == "gif")
                        return CT_IMAGE_GIF;
                    break;
                case 'i':
                    if (extName == "ico")
                        return CT_IMAGE_XICON;
                    break;
                case 'j':
                    if (extName == "jpg")
                        return CT_IMAGE_JPG;
                    break;
                case 'o':
                    if (extName == "otf")
                        return CT_APPLICATION_X_FONT_OPENTYPE;
                    break;
                case 'p':
                    if (extName == "png")
                        return CT_IMAGE_PNG;
                    else if (extName == "pdf")
                        return CT_APPLICATION_PDF;
                    break;
                case 's':
                    if (extName == "svg")
                        return CT_IMAGE_SVG_XML;
                    break;
                case 't':
                    if (extName == "txt")
                        return CT_TEXT_PLAIN;
                    else if (extName == "ttf")
                        return CT_APPLICATION_X_FONT_TRUETYPE;
                    break;
                case 'x':
                    if (extName == "xml")
                        return CT_TEXT_XML;
                    else if (extName == "xsl")
                        return CT_TEXT_XSL;
                    break;
                default:
                    break;
            }
            return CT_APPLICATION_OCTET_STREAM;
        }
        case 4:
        {
            if (extName == "avif")
                return CT_IMAGE_AVIF;
            else if (extName == "html")
                return CT_TEXT_HTML;
            else if (extName == "jpeg")
                return CT_IMAGE_JPG;
            else if (extName == "icns")
                return CT_IMAGE_ICNS;
            else if (extName == "webp")
                return CT_IMAGE_WEBP;
            else if (extName == "wasm")
                return CT_APPLICATION_WASM;
            else if (extName == "woff")
                return CT_APPLICATION_FONT_WOFF;
            return CT_APPLICATION_OCTET_STREAM;
        }
        case 5:
        {
            if (extName == "woff2")
                return CT_APPLICATION_FONT_WOFF2;
            return CT_APPLICATION_OCTET_STREAM;
        }
        default:
            return CT_APPLICATION_OCTET_STREAM;
    }
}

ContentType parseContentType(const std::string_view &contentType)
{
    static const std::unordered_map<std::string_view, ContentType> map_{
        {"text/html", CT_TEXT_HTML},
        {"application/x-www-form-urlencoded", CT_APPLICATION_X_FORM},
        {"application/xml", CT_APPLICATION_XML},
        {"application/json", CT_APPLICATION_JSON},
        {"application/x-javascript", CT_APPLICATION_X_JAVASCRIPT},
        {"text/javascript", CT_TEXT_JAVASCRIPT},
        {"text/css", CT_TEXT_CSS},
        {"text/xml", CT_TEXT_XML},
        {"text/xsl", CT_TEXT_XSL},
        {"application/octet-stream", CT_APPLICATION_OCTET_STREAM},
        {"image/svg+xml", CT_IMAGE_SVG_XML},
        {"application/x-font-truetype", CT_APPLICATION_X_FONT_TRUETYPE},
        {"application/x-font-opentype", CT_APPLICATION_X_FONT_OPENTYPE},
        {"application/font-woff", CT_APPLICATION_FONT_WOFF},
        {"application/font-woff2", CT_APPLICATION_FONT_WOFF2},
        {"application/vnd.ms-fontobject", CT_APPLICATION_VND_MS_FONTOBJ},
        {"application/pdf", CT_APPLICATION_PDF},
        {"image/png", CT_IMAGE_PNG},
        {"image/webp", CT_IMAGE_WEBP},
        {"image/avif", CT_IMAGE_AVIF},
        {"image/jpeg", CT_IMAGE_JPG},
        {"image/gif", CT_IMAGE_GIF},
        {"image/x-icon", CT_IMAGE_XICON},
        {"image/bmp", CT_IMAGE_BMP},
        {"image/icns", CT_IMAGE_ICNS},
        {"application/wasm", CT_APPLICATION_WASM},
        {"text/plain", CT_TEXT_PLAIN},
        {"multipart/form-data", CT_MULTIPART_FORM_DATA}};
    auto iter = map_.find(contentType);
    if (iter == map_.end())
        return CT_NONE;
    return iter->second;
}

FileType parseFileType(const std::string_view &fileExtension)
{
    // https://en.wikipedia.org/wiki/List_of_file_formats
    static const std::unordered_map<std::string_view, FileType> map_{
        {"", FT_UNKNOWN},    {"html", FT_DOCUMENT}, {"docx", FT_DOCUMENT},
        {"zip", FT_ARCHIVE}, {"rar", FT_ARCHIVE},   {"xz", FT_ARCHIVE},
        {"7z", FT_ARCHIVE},  {"tgz", FT_ARCHIVE},   {"gz", FT_ARCHIVE},
        {"bz2", FT_ARCHIVE}, {"mp3", FT_AUDIO},     {"wav", FT_AUDIO},
        {"avi", FT_MEDIA},   {"gif", FT_MEDIA},     {"mp4", FT_MEDIA},
        {"mov", FT_MEDIA},   {"png", FT_IMAGE},     {"jpeg", FT_IMAGE},
        {"jpg", FT_IMAGE},   {"ico", FT_IMAGE}};
    auto iter = map_.find(fileExtension);
    if (iter == map_.end())
        return FT_CUSTOM;
    return iter->second;
}

const std::string_view &contentTypeToMime(ContentType contenttype)
{
    switch (contenttype)
    {
        case CT_TEXT_HTML:
        {
            static std::string_view sv = "text/html; charset=utf-8";
            return sv;
        }
        case CT_APPLICATION_X_FORM:
        {
            static std::string_view sv = "application/x-www-form-urlencoded";
            return sv;
        }
        case CT_APPLICATION_XML:
        {
            static std::string_view sv = "application/xml; charset=utf-8";
            return sv;
        }
        case CT_APPLICATION_JSON:
        {
            static std::string_view sv = "application/json; charset=utf-8";
            return sv;
        }
        case CT_APPLICATION_X_JAVASCRIPT:
        {
            static std::string_view sv =
                "application/x-javascript; charset=utf-8";
            return sv;
        }
        case CT_TEXT_JAVASCRIPT:
        {
            static std::string_view sv = "text/javascript; charset=utf-8";
            return sv;
        }
        case CT_TEXT_CSS:
        {
            static std::string_view sv = "text/css; charset=utf-8";
            return sv;
        }
        case CT_TEXT_XML:
        {
            static std::string_view sv = "text/xml; charset=utf-8";
            return sv;
        }
        case CT_TEXT_XSL:
        {
            static std::string_view sv = "text/xsl; charset=utf-8";
            return sv;
        }
        case CT_APPLICATION_OCTET_STREAM:
        {
            static std::string_view sv = "application/octet-stream";
            return sv;
        }
        case CT_IMAGE_SVG_XML:
        {
            static std::string_view sv = "image/svg+xml";
            return sv;
        }
        case CT_APPLICATION_X_FONT_TRUETYPE:
        {
            static std::string_view sv = "application/x-font-truetype";
            return sv;
        }
        case CT_APPLICATION_X_FONT_OPENTYPE:
        {
            static std::string_view sv = "application/x-font-opentype";
            return sv;
        }
        case CT_APPLICATION_FONT_WOFF:
        {
            static std::string_view sv = "application/font-woff";
            return sv;
        }
        case CT_APPLICATION_FONT_WOFF2:
        {
            static std::string_view sv = "application/font-woff2";
            return sv;
        }
        case CT_APPLICATION_VND_MS_FONTOBJ:
        {
            static std::string_view sv = "application/vnd.ms-fontobject";
            return sv;
        }
        case CT_APPLICATION_PDF:
        {
            static std::string_view sv = "application/pdf";
            return sv;
        }
        case CT_IMAGE_PNG:
        {
            static std::string_view sv = "image/png";
            return sv;
        }
        case CT_IMAGE_AVIF:
        {
            static std::string_view sv = "image/avif";
            return sv;
        }
        case CT_IMAGE_WEBP:
        {
            static std::string_view sv = "image/webp";
            return sv;
        }
        case CT_IMAGE_JPG:
        {
            static std::string_view sv = "image/jpeg";
            return sv;
        }
        case CT_IMAGE_GIF:
        {
            static std::string_view sv = "image/gif";
            return sv;
        }
        case CT_IMAGE_XICON:
        {
            static std::string_view sv = "image/x-icon";
            return sv;
        }
        case CT_IMAGE_BMP:
        {
            static std::string_view sv = "image/bmp";
            return sv;
        }
        case CT_IMAGE_ICNS:
        {
            static std::string_view sv = "image/icns";
            return sv;
        }
        case CT_APPLICATION_WASM:
        {
            static std::string_view sv = "application/wasm";
            return sv;
        }
        case CT_NONE:
        {
            static std::string_view sv = "";
            return sv;
        }
        default:
        case CT_TEXT_PLAIN:
        {
            static std::string_view sv = "text/plain; charset=utf-8";
            return sv;
        }
    }
}

void registerCustomExtensionMime(const std::string &ext,
                                 const std::string &mime)
{
    if (ext.empty())
        return;
    auto &mimeStr = customMime[ext];
    if (!mimeStr.empty())
    {
        LOG_WARN << ext << " has already been registered as type " << mime
                 << ". Overwriting.";
    }
    mimeStr = mime;
}

const std::string_view fileNameToMime(const std::string &fileName)
{
    ContentType intenalContentType = getContentType(fileName);
    if (intenalContentType != CT_APPLICATION_OCTET_STREAM)
        return contentTypeToMime(intenalContentType);

    std::string extName;
    auto pos = fileName.rfind('.');
    if (pos != std::string::npos)
    {
        extName = fileName.substr(pos + 1);
        transform(extName.begin(),
                  extName.end(),
                  extName.begin(),
                  [](unsigned char c) { return tolower(c); });
    }
    auto it = customMime.find(extName);
    if (it == customMime.end())
        return "";
    return it->second;
}

std::pair<ContentType, const std::string_view> fileNameToContentTypeAndMime(
    const std::string &fileName)
{
    ContentType intenalContentType = getContentType(fileName);
    if (intenalContentType != CT_APPLICATION_OCTET_STREAM)
        return {intenalContentType, contentTypeToMime(intenalContentType)};

    std::string extName;
    auto pos = fileName.rfind('.');
    if (pos != std::string::npos)
    {
        extName = fileName.substr(pos + 1);
        transform(extName.begin(),
                  extName.end(),
                  extName.begin(),
                  [](unsigned char c) { return tolower(c); });
    }
    auto it = customMime.find(extName);
    if (it == customMime.end())
        return {CT_NONE, ""};
    return {CT_CUSTOM, it->second};
}
}  // namespace drogon
