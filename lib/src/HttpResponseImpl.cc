// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include "HttpResponseImpl.h"

#include <drogon/HttpViewBase.h>
#include <drogon/HttpViewData.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <stdio.h>

using namespace trantor;
using namespace drogon;

HttpResponsePtr HttpResponse::newHttpResponse()
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(HttpResponse::k200Ok);
    res->setContentTypeCode(CT_TEXT_HTML);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(const Json::Value &data)
{
    auto res=std::make_shared<HttpResponseImpl>();
    res->setStatusCode(HttpResponse::k200Ok);
    res->setContentTypeCode(CT_APPLICATION_JSON);
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    res->setBody(writeString(builder, data));
    return res;
}
HttpResponsePtr HttpResponse::notFoundResponse()
{
    HttpViewData data;
    data.insert("version",getVersion());
    auto res=HttpResponse::newHttpViewResponse("NotFound",data);
    res->setStatusCode(HttpResponse::k404NotFound);
    //res->setCloseConnection(true);

    return res;
}
HttpResponsePtr HttpResponse::locationRedirectResponse(std::string path)
{
    auto res=std::make_shared<HttpResponseImpl>();
    res->setStatusCode(HttpResponse::k302Found);
    res->redirect(path.c_str());
    return res;
}

HttpResponsePtr HttpResponse::newHttpViewResponse(const std::string &viewName,const HttpViewData& data)
{
    return HttpViewBase::genHttpResponse(viewName,data);
}
const std::string HttpResponseImpl::web_content_type_to_string(uint8_t contenttype)
{
    switch(contenttype) {
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


const std::string HttpResponseImpl::web_response_code_to_string(int code)
{
    switch(code) {
        case 200:
            return "OK";

        case 304:
            return "Not Modified";
        case 307:
            return "Temporary Redirect";

        case 400:
            return "Bad Request";

        case 403:
            return "Forbidden";

        case 404:
            return "Not Found";

        case 412:
            return "Preconditions Failed";

        default:
            if(code >= 100 && code < 200)
                return "Informational";

            if(code >= 200 && code < 300)
                return "Successful";

            if(code >= 300 && code < 400)
                return "Redirection";

            if(code >= 400 && code < 500)
                return "Bad Request";

            if(code >= 500 && code < 600)
                return "Server Error";

            return "Undefined Error";
    }
}
void HttpResponseImpl::appendToBuffer(MsgBuffer* output) const
{
    char buf[32];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", _statusCode);
    output->append(buf);
    output->append(_statusMessage);
    output->append("\r\n");
    snprintf(buf, sizeof buf, "Content-Length: %lu\r\n", _body.size());
    output->append(buf);
    if(_headers.find("Connection")==_headers.end())
    {
        if (_closeConnection) {
            output->append("Connection: close\r\n");
        } else {

            output->append("Connection: Keep-Alive\r\n");
        }
    }


    for (auto it = _headers.begin();
         it != _headers.end();
         ++it) {
        output->append(it->first);
        output->append(": ");
        output->append(it->second);
        output->append("\r\n");
    }
    if(_cookies.size() > 0) {
        for(auto it = _cookies.begin(); it != _cookies.end(); it++) {

             output->append(it->second.cookieString());
        }
    }

    output->append("\r\n");

	LOG_TRACE<<"reponse(no body):"<<output->peek();
	output->append(_body);
	
}
