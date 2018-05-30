// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.


//taken from muduo and modified
//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.



#pragma once

#include <drogon/HttpViewData.h>
#include <json/json.h>
#include <string>
#include <memory>

using std::string;
#define CT_APPLICATION_JSON				1
#define CT_TEXT_PLAIN					2
#define CT_TEXT_HTML					3
#define CT_APPLICATION_X_JAVASCRIPT		4
#define CT_TEXT_CSS						5
#define CT_TEXT_XML						6
#define CT_APPLICATION_XML				7
#define CT_TEXT_XSL						8
#define CT_APPLICATION_OCTET_STREAM		9
#define CT_APPLICATION_X_FONT_TRUETYPE	10
#define CT_APPLICATION_X_FONT_OPENTYPE	11
#define CT_APPLICATION_FONT_WOFF		12
#define CT_APPLICATION_FONT_WOFF2		13
#define CT_APPLICATION_VND_MS_FONTOBJ	14
#define CT_IMAGE_SVG_XML				15
#define CT_IMAGE_PNG					16
#define CT_IMAGE_JPG					17
#define CT_IMAGE_GIF					18
#define CT_IMAGE_XICON					19
#define CT_IMAGE_ICNS					20
#define CT_IMAGE_BMP					21

namespace drogon
{
    class HttpResponse;
    typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
    class HttpResponse
    {
    public:
        enum HttpStatusCode {
            kUnknown,
            k200Ok = 200,
            k301MovedPermanently = 301,
            k302Found = 302,
            k304NotModified = 304,
            k400BadRequest = 400,
            k404NotFound = 404,
        };

        enum Version {
            kHttp10, kHttp11
        };

        explicit HttpResponse()
        {
        }

        virtual void setStatusCode(HttpStatusCode code)=0;

        virtual void setStatusCode(HttpStatusCode code, const std::string& status_message)=0;

        virtual void setVersion(const Version v)=0;

        virtual void setCloseConnection(bool on)=0;

        virtual bool closeConnection() const =0;

        virtual void setContentTypeCode(uint8_t type)=0;

        virtual std::string getHeader(const std::string& key)=0;

        virtual void addHeader(const std::string& key, const std::string& value)=0;

        virtual void addHeader(const char* start, const char* colon, const char* end)=0;

        virtual void addCookie(const std::string& key, const std::string& value)=0;

        virtual void setBody(const std::string& body)=0;

        virtual void setBody(std::string&& body)=0;

        virtual void redirect(const std::string& url)=0;

        virtual void clear()=0;

        virtual std::string getBody() const=0;

        static HttpResponsePtr newHttpResponse();
        static HttpResponsePtr notFoundResponse();
        static HttpResponsePtr newHttpJsonResponse(const Json::Value &data);
        static HttpResponsePtr newHttpViewResponse(const std::string &viewName,const HttpViewData& data);
        static HttpResponsePtr locationRedirectResponse(std::string path);
    };

}


