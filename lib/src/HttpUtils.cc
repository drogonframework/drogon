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

#include "HttpUtils.h"
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>

namespace drogon
{
const string_view &webContentTypeToString(ContentType contenttype)
{
    switch (contenttype)
    {
        case CT_TEXT_HTML:
        {
            static string_view sv = "Content-Type: text/html; charset=utf-8\r\n";
            return sv;
        }
        case CT_APPLICATION_X_FORM:
        {
            static string_view sv = "Content-Type: application/x-www-form-urlencoded\r\n";
            return sv;
        }
        case CT_APPLICATION_XML:
        {
            static string_view sv = "Content-Type: application/xml; charset=utf-8\r\n";
            return sv;
        }
        case CT_APPLICATION_JSON:
        {
            static string_view sv = "Content-Type: application/json; charset=utf-8\r\n";
            return sv;
        }
        case CT_APPLICATION_X_JAVASCRIPT:
        {
            static string_view sv = "Content-Type: application/x-javascript; charset=utf-8\r\n";
            return sv;
        }
        case CT_TEXT_CSS:
        {
            static string_view sv = "Content-Type: text/css; charset=utf-8\r\n";
            return sv;
        }
        case CT_TEXT_XML:
        {
            static string_view sv = "Content-Type: text/xml; charset=utf-8\r\n";
            return sv;
        }
        case CT_TEXT_XSL:
        {
            static string_view sv = "Content-Type: text/xsl; charset=utf-8\r\n";
            return sv;
        }
        case CT_APPLICATION_OCTET_STREAM:
        {
            static string_view sv = "Content-Type: application/octet-stream\r\n";
            return sv;
        }
        case CT_IMAGE_SVG_XML:
        {
            static string_view sv = "Content-Type: image/svg+xml\r\n";
            return sv;
        }
        case CT_APPLICATION_X_FONT_TRUETYPE:
        {
            static string_view sv = "Content-Type: application/x-font-truetype\r\n";
            return sv;
        }
        case CT_APPLICATION_X_FONT_OPENTYPE:
        {
            static string_view sv = "Content-Type: application/x-font-opentype\r\n";
            return sv;
        }
        case CT_APPLICATION_FONT_WOFF:
        {
            static string_view sv = "Content-Type: application/font-woff\r\n";
            return sv;
        }
        case CT_APPLICATION_FONT_WOFF2:
        {
            static string_view sv = "Content-Type: application/font-woff2\r\n";
            return sv;
        }
        case CT_APPLICATION_VND_MS_FONTOBJ:
        {
            static string_view sv = "Content-Type: application/vnd.ms-fontobject\r\n";
            return sv;
        }
        case CT_IMAGE_PNG:
        {
            static string_view sv = "Content-Type: image/png\r\n";
            return sv;
        }
        case CT_IMAGE_JPG:
        {
            static string_view sv = "Content-Type: image/jpeg\r\n";
            return sv;
        }
        case CT_IMAGE_GIF:
        {
            static string_view sv = "Content-Type: image/gif\r\n";
            return sv;
        }
        case CT_IMAGE_XICON:
        {
            static string_view sv = "Content-Type: image/x-icon\r\n";
            return sv;
        }
        case CT_IMAGE_BMP:
        {
            static string_view sv = "Content-Type: image/bmp\r\n";
            return sv;
        }
        case CT_IMAGE_ICNS:
        {
            static string_view sv = "Content-Type: image/icns\r\n";
            return sv;
        }
        default:
        case CT_TEXT_PLAIN:
        {
            static string_view sv = "Content-Type: text/plain; charset=utf-8\r\n";
            return sv;
        }
    }
}

const string_view &statusCodeToString(int code)
{
    switch (code)
    {
        case 100:
        {
            static string_view sv = "Continue";
            return sv;
        }
        case 101:
        {
            static string_view sv = "Switching Protocols";
            return sv;
        }
        case 200:
        {
            static string_view sv = "OK";
            return sv;
        }
        case 201:
        {
            static string_view sv = "Created";
            return sv;
        }
        case 202:
        {
            static string_view sv = "Accepted";
            return sv;
        }
        case 203:
        {
            static string_view sv = "Non-Authoritative Information";
            return sv;
        }
        case 204:
        {
            static string_view sv = "No Content";
            return sv;
        }
        case 205:
        {
            static string_view sv = "Reset Content";
            return sv;
        }
        case 206:
        {
            static string_view sv = "Partial Content";
            return sv;
        }
        case 300:
        {
            static string_view sv = "Multiple Choices";
            return sv;
        }
        case 301:
        {
            static string_view sv = "Moved Permanently";
            return sv;
        }
        case 302:
        {
            static string_view sv = "Found";
            return sv;
        }
        case 303:
        {
            static string_view sv = "See Other";
            return sv;
        }
        case 304:
        {
            static string_view sv = "Not Modified";
            return sv;
        }
        case 305:
        {
            static string_view sv = "Use Proxy";
            return sv;
        }
        case 307:
        {
            static string_view sv = "Temporary Redirect";
            return sv;
        }
        case 400:
        {
            static string_view sv = "Bad Request";
            return sv;
        }
        case 401:
        {
            static string_view sv = "Unauthorized";
            return sv;
        }
        case 402:
        {
            static string_view sv = "Payment Required";
            return sv;
        }
        case 403:
        {
            static string_view sv = "Forbidden";
            return sv;
        }
        case 404:
        {
            static string_view sv = "Not Found";
            return sv;
        }
        case 405:
        {
            static string_view sv = "Method Not Allowed";
            return sv;
        }
        case 406:
        {
            static string_view sv = "Not Acceptable";
            return sv;
        }
        case 407:
        {
            static string_view sv = "Proxy Authentication Required";
            return sv;
        }
        case 408:
        {
            static string_view sv = "Request Time-out";
            return sv;
        }
        case 409:
        {
            static string_view sv = "Conflict";
            return sv;
        }
        case 410:
        {
            static string_view sv = "Gone";
            return sv;
        }
        case 411:
        {
            static string_view sv = "Length Required";
            return sv;
        }
        case 412:
        {
            static string_view sv = "Precondition Failed";
            return sv;
        }
        case 413:
        {
            static string_view sv = "Request Entity Too Large";
            return sv;
        }
        case 414:
        {
            static string_view sv = "Request-URI Too Large";
            return sv;
        }
        case 415:
        {
            static string_view sv = "Unsupported Media Type";
            return sv;
        }
        case 416:
        {
            static string_view sv = "Requested range not satisfiable";
            return sv;
        }
        case 417:
        {
            static string_view sv = "Expectation Failed";
            return sv;
        }
        case 500:
        {
            static string_view sv = "Internal Server Error";
            return sv;
        }
        case 501:
        {
            static string_view sv = "Not Implemented";
            return sv;
        }
        case 502:
        {
            static string_view sv = "Bad Gateway";
            return sv;
        }
        case 503:
        {
            static string_view sv = "Service Unavailable";
            return sv;
        }
        case 504:
        {
            static string_view sv = "Gateway Time-out";
            return sv;
        }
        case 505:
        {
            static string_view sv = "HTTP Version not supported";
            return sv;
        }
        default:
            if (code >= 100 && code < 200)
            {
                static string_view sv = "Informational";
                return sv;
            }
            else if (code >= 200 && code < 300)
            {
                static string_view sv = "Successful";
                return sv;
            }
            else if (code >= 300 && code < 400)
            {
                static string_view sv = "Redirection";
                return sv;
            }
            else if (code >= 400 && code < 500)
            {
                static string_view sv = "Bad Request";
                return sv;
            }
            else if (code >= 500 && code < 600)
            {
                static string_view sv = "Server Error";
                return sv;
            }
            else
            {
                static string_view sv = "Undefined Error";
                return sv;
            }
    }
}

ContentType getContentType(const std::string &filename)
{
    std::string extName;
    auto pos = filename.rfind(".");
    if (pos != std::string::npos)
    {
        extName = filename.substr(pos + 1);
        transform(extName.begin(), extName.end(), extName.begin(), tolower);
    }
    switch (extName.length())
    {
        case 0:
            return CT_APPLICATION_OCTET_STREAM;
        case 2:
        {
            if (extName == "js")
                return CT_APPLICATION_X_JAVASCRIPT;
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
            if (extName == "html")
                return CT_TEXT_HTML;
            else if (extName == "jpeg")
                return CT_IMAGE_JPG;
            else if (extName == "icns")
                return CT_IMAGE_ICNS;
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

}  // namespace drogon