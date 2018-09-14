// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

/**
 *  taken from muduo and modified
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



#pragma once

#include <drogon/HttpViewData.h>
#include <drogon/Cookie.h>
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
            //rfc2616-6.1.1
            kUnknown=0,
            k100Continue=100,
            k101SwitchingProtocols=101,
            k200OK=200,
            k201Created=201,
            k202Accepted=202,
            k203NonAuthoritativeInformation=203,
            k204NoContent=204,
            k205ResetContent=205,
            k206PartialContent=206,
            k300MultipleChoices=300,
            k301MovedPermanently=301,
            k302Found=302,
            k303SeeOther=303,
            k304NotModified=304,
            k305UseProxy=305,
            k307TemporaryRedirect=307,
            k400BadRequest=400,
            k401Unauthorized=401,
            k402PaymentRequired=402,
            k403Forbidden=403,
            k404NotFound=404,
            k405MethodNotAllowed=405,
            k406NotAcceptable=406,
            k407ProxyAuthenticationRequired=407,
            k408RequestTimeout=408,
            k409Conflict=409,
            k410Gone=410,
            k411LengthRequired=411,
            k412PreconditionFailed=412,
            k413RequestEntityTooLarge=413,
            k414RequestURITooLarge=414,
            k415UnsupportedMediaType=415,
            k416Requestedrangenotsatisfiable=416,
            k417ExpectationFailed=417,
            k500InternalServerError=500,
            k501NotImplemented=501,
            k502BadGateway=502,
            k503ServiceUnavailable=503,
            k504GatewayTimeout=504,
            k505HTTPVersionnotsupported=505,
        };

        enum Version {
            kHttp10, kHttp11
        };

        explicit HttpResponse()
        {
        }

        virtual HttpStatusCode statusCode()=0;
        virtual void setStatusCode(HttpStatusCode code)=0;

        virtual void setStatusCode(HttpStatusCode code, const std::string& status_message)=0;

        virtual void setVersion(const Version v)=0;

        virtual void setCloseConnection(bool on)=0;

        virtual bool closeConnection() const =0;

        virtual void setContentTypeCode(uint8_t type)=0;

        virtual void setContentTypeCodeAndCharacterSet(uint8_t type,const std::string charSet="utf-8")=0;

        virtual uint8_t getContentTypeCode()=0;

        virtual std::string getHeader(const std::string& key) const =0;

        virtual void addHeader(const std::string& key, const std::string& value)=0;

        virtual void addHeader(const std::string& key, std::string&& value)=0;

        virtual void addHeader(const char* start, const char* colon, const char* end)=0;

        virtual void addCookie(const std::string& key, const std::string& value)=0;

        virtual void addCookie(const Cookie &cookie)=0;

        virtual void setBody(const std::string& body)=0;

        virtual void setBody(std::string&& body)=0;

        virtual void redirect(const std::string& url)=0;

        virtual void clear()=0;

        virtual const std::string & getBody() const=0;
        virtual std::string & getBody() = 0;

        virtual const std::shared_ptr<Json::Value> getJsonObject() const =0;
        static HttpResponsePtr newHttpResponse();
        static HttpResponsePtr newNotFoundResponse();
        static HttpResponsePtr newHttpJsonResponse(const Json::Value &data);
        static HttpResponsePtr newHttpViewResponse(const std::string &viewName,const HttpViewData& data);
        static HttpResponsePtr newLocationRedirectResponse(const std::string &path);
    };

}


