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

namespace drogon
{

// std::string webContentTypeAndCharsetToString(ContentType contenttype, const std::string &charSet)
// {
//     switch (contenttype)
//     {
//     case CT_TEXT_HTML:
//         return "text/html; charset=" + charSet;

//     case CT_APPLICATION_X_FORM:
//         return "application/x-www-form-urlencoded";

//     case CT_APPLICATION_XML:
//         return "application/xml; charset=" + charSet;

//     case CT_APPLICATION_JSON:
//         return "application/json; charset=" + charSet;

//     case CT_APPLICATION_X_JAVASCRIPT:
//         return "application/x-javascript; charset=" + charSet;

//     case CT_TEXT_CSS:
//         return "text/css; charset=" + charSet;

//     case CT_TEXT_XML:
//         return "text/xml; charset=" + charSet;

//     case CT_TEXT_XSL:
//         return "text/xsl; charset=" + charSet;

//     case CT_APPLICATION_OCTET_STREAM:
//         return "application/octet-stream";

//     case CT_IMAGE_SVG_XML:
//         return "image/svg+xml";

//     case CT_APPLICATION_X_FONT_TRUETYPE:
//         return "application/x-font-truetype";

//     case CT_APPLICATION_X_FONT_OPENTYPE:
//         return "application/x-font-opentype";

//     case CT_APPLICATION_FONT_WOFF:
//         return "application/font-woff";

//     case CT_APPLICATION_FONT_WOFF2:
//         return "application/font-woff2";

//     case CT_APPLICATION_VND_MS_FONTOBJ:
//         return "application/vnd.ms-fontobject";

//     case CT_IMAGE_PNG:
//         return "image/png";

//     case CT_IMAGE_JPG:
//         return "image/jpeg";

//     case CT_IMAGE_GIF:
//         return "image/gif";

//     case CT_IMAGE_XICON:
//         return "image/x-icon";

//     case CT_IMAGE_BMP:
//         return "image/bmp";

//     case CT_IMAGE_ICNS:
//         return "image/icns";

//     default:
//     case CT_TEXT_PLAIN:
//         return "text/plain; charset=" + charSet;
//     }
// }
#if CXX_STD >= 17
const std::string_view &webContentTypeToString(ContentType contenttype)
{
    switch (contenttype)
    {
    case CT_TEXT_HTML:
    {
        static std::string_view sv = "Content-Type: text/html; charset=utf-8\r\n";
        return sv;
    }
    case CT_APPLICATION_X_FORM:
    {
        static std::string_view sv = "Content-Type: application/x-www-form-urlencoded\r\n";
        return sv;
    }
    case CT_APPLICATION_XML:
    {
        static std::string_view sv = "Content-Type: application/xml; charset=utf-8\r\n";
        return sv;
    }
    case CT_APPLICATION_JSON:
    {
        static std::string_view sv = "Content-Type: application/json; charset=utf-8\r\n";
        return sv;
    }
    case CT_APPLICATION_X_JAVASCRIPT:
    {
        static std::string_view sv = "Content-Type: application/x-javascript; charset=utf-8\r\n";
        return sv;
    }
    case CT_TEXT_CSS:
    {
        static std::string_view sv = "Content-Type: text/css; charset=utf-8\r\n";
        return sv;
    }
    case CT_TEXT_XML:
    {
        static std::string_view sv = "Content-Type: text/xml; charset=utf-8\r\n";
        return sv;
    }
    case CT_TEXT_XSL:
    {
        static std::string_view sv = "Content-Type: text/xsl; charset=utf-8\r\n";
        return sv;
    }
    case CT_APPLICATION_OCTET_STREAM:
    {
        static std::string_view sv = "Content-Type: application/octet-stream\r\n";
        return sv;
    }
    case CT_IMAGE_SVG_XML:
    {
        static std::string_view sv = "Content-Type: image/svg+xml\r\n";
        return sv;
    }
    case CT_APPLICATION_X_FONT_TRUETYPE:
    {
        static std::string_view sv = "Content-Type: application/x-font-truetype\r\n";
        return sv;
    }
    case CT_APPLICATION_X_FONT_OPENTYPE:
    {
        static std::string_view sv = "Content-Type: application/x-font-opentype\r\n";
        return sv;
    }
    case CT_APPLICATION_FONT_WOFF:
    {
        static std::string_view sv = "Content-Type: application/font-woff\r\n";
        return sv;
    }
    case CT_APPLICATION_FONT_WOFF2:
    {
        static std::string_view sv = "Content-Type: application/font-woff2\r\n";
        return sv;
    }
    case CT_APPLICATION_VND_MS_FONTOBJ:
    {
        static std::string_view sv = "Content-Type: application/vnd.ms-fontobject\r\n";
        return sv;
    }
    case CT_IMAGE_PNG:
    {
        static std::string_view sv = "Content-Type: image/png\r\n";
        return sv;
    }
    case CT_IMAGE_JPG:
    {
        static std::string_view sv = "Content-Type: image/jpeg\r\n";
        return sv;
    }
    case CT_IMAGE_GIF:
    {
        static std::string_view sv = "Content-Type: image/gif\r\n";
        return sv;
    }
    case CT_IMAGE_XICON:
    {
        static std::string_view sv = "Content-Type: image/x-icon\r\n";
        return sv;
    }
    case CT_IMAGE_BMP:
    {
        static std::string_view sv = "Content-Type: image/bmp\r\n";
        return sv;
    }
    case CT_IMAGE_ICNS:
    {
        static std::string_view sv = "Content-Type: image/icns\r\n";
        return sv;
    }
    default:
    case CT_TEXT_PLAIN:
    {
        static std::string_view sv = "Content-Type: text/plain; charset=utf-8\r\n";
        return sv;
    }
    }
}
#else
const char *webContentTypeToString(ContentType contenttype)
{
    switch (contenttype)
    {
    case CT_TEXT_HTML:
        return "Content-Type: text/html; charset=utf-8\r\n";

    case CT_APPLICATION_X_FORM:
        return "Content-Type: application/x-www-form-urlencoded\r\n";

    case CT_APPLICATION_XML:
        return "Content-Type: application/xml; charset=utf-8\r\n";

    case CT_APPLICATION_JSON:
        return "Content-Type: application/json; charset=utf-8\r\n";

    case CT_APPLICATION_X_JAVASCRIPT:
        return "Content-Type: application/x-javascript; charset=utf-8\r\n";

    case CT_TEXT_CSS:
        return "Content-Type: text/css; charset=utf-8\r\n";

    case CT_TEXT_XML:
        return "Content-Type: text/xml; charset=utf-8\r\n";

    case CT_TEXT_XSL:
        return "Content-Type: text/xsl; charset=utf-8\r\n";

    case CT_APPLICATION_OCTET_STREAM:
        return "Content-Type: application/octet-stream\r\n";

    case CT_IMAGE_SVG_XML:
        return "Content-Type: image/svg+xml\r\n";

    case CT_APPLICATION_X_FONT_TRUETYPE:
        return "Content-Type: application/x-font-truetype\r\n";

    case CT_APPLICATION_X_FONT_OPENTYPE:
        return "Content-Type: application/x-font-opentype\r\n";

    case CT_APPLICATION_FONT_WOFF:
        return "Content-Type: application/font-woff\r\n";

    case CT_APPLICATION_FONT_WOFF2:
        return "Content-Type: application/font-woff2\r\n";

    case CT_APPLICATION_VND_MS_FONTOBJ:
        return "Content-Type: application/vnd.ms-fontobject\r\n";

    case CT_IMAGE_PNG:
        return "Content-Type: image/png\r\n";

    case CT_IMAGE_JPG:
        return "Content-Type: image/jpeg\r\n";

    case CT_IMAGE_GIF:
        return "Content-Type: image/gif\r\n";

    case CT_IMAGE_XICON:
        return "Content-Type: image/x-icon\r\n";

    case CT_IMAGE_BMP:
        return "Content-Type: image/bmp\r\n";

    case CT_IMAGE_ICNS:
        return "Content-Type: image/icns\r\n";

    default:
    case CT_TEXT_PLAIN:
        return "Content-Type: text/plain; charset=utf-8\r\n";
    }
}
#endif
} // namespace drogon