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

std::string webContentTypeAndCharsetToString(ContentType contenttype, const std::string &charSet)
{
    switch (contenttype)
    {
    case CT_TEXT_HTML:
        return "text/html; charset=" + charSet;

    case CT_APPLICATION_XML:
        return "application/xml; charset=" + charSet;

    case CT_APPLICATION_JSON:
        return "application/json; charset=" + charSet;

    case CT_APPLICATION_X_JAVASCRIPT:
        return "application/x-javascript; charset=" + charSet;

    case CT_TEXT_CSS:
        return "text/css; charset=" + charSet;

    case CT_TEXT_XML:
        return "text/xml; charset=" + charSet;

    case CT_TEXT_XSL:
        return "text/xsl; charset=" + charSet;

    case CT_APPLICATION_OCTET_STREAM:
        return "application/octet-stream";

    case CT_IMAGE_SVG_XML:
        return "image/svg+xml";

    case CT_APPLICATION_X_FONT_TRUETYPE:
        return "application/x-font-truetype";

    case CT_APPLICATION_X_FONT_OPENTYPE:
        return "application/x-font-opentype";

    case CT_APPLICATION_FONT_WOFF:
        return "application/font-woff";

    case CT_APPLICATION_FONT_WOFF2:
        return "application/font-woff2";

    case CT_APPLICATION_VND_MS_FONTOBJ:
        return "application/vnd.ms-fontobject";

    case CT_IMAGE_PNG:
        return "image/png";

    case CT_IMAGE_JPG:
        return "image/jpeg";

    case CT_IMAGE_GIF:
        return "image/gif";

    case CT_IMAGE_XICON:
        return "image/x-icon";

    case CT_IMAGE_BMP:
        return "image/bmp";

    case CT_IMAGE_ICNS:
        return "image/icns";

    default:
    case CT_TEXT_PLAIN:
        return "text/plain; charset=" + charSet;
    }
}

std::string webContentTypeToString(ContentType contenttype)
{
    switch (contenttype)
    {
    case CT_TEXT_HTML:
        return "text/html; charset=utf-8";

    case CT_APPLICATION_XML:
        return "application/xml; charset=utf-8";

    case CT_APPLICATION_JSON:
        return "application/json; charset=utf-8";

    case CT_APPLICATION_X_JAVASCRIPT:
        return "application/x-javascript; charset=utf-8";

    case CT_TEXT_CSS:
        return "text/css; charset=utf-8";

    case CT_TEXT_XML:
        return "text/xml; charset=utf-8";

    case CT_TEXT_XSL:
        return "text/xsl; charset=utf-8";

    case CT_APPLICATION_OCTET_STREAM:
        return "application/octet-stream";

    case CT_IMAGE_SVG_XML:
        return "image/svg+xml";

    case CT_APPLICATION_X_FONT_TRUETYPE:
        return "application/x-font-truetype";

    case CT_APPLICATION_X_FONT_OPENTYPE:
        return "application/x-font-opentype";

    case CT_APPLICATION_FONT_WOFF:
        return "application/font-woff";

    case CT_APPLICATION_FONT_WOFF2:
        return "application/font-woff2";

    case CT_APPLICATION_VND_MS_FONTOBJ:
        return "application/vnd.ms-fontobject";

    case CT_IMAGE_PNG:
        return "image/png";

    case CT_IMAGE_JPG:
        return "image/jpeg";

    case CT_IMAGE_GIF:
        return "image/gif";

    case CT_IMAGE_XICON:
        return "image/x-icon";

    case CT_IMAGE_BMP:
        return "image/bmp";

    case CT_IMAGE_ICNS:
        return "image/icns";

    default:
    case CT_TEXT_PLAIN:
        return "text/plain; charset=utf-8";
    }
}

}