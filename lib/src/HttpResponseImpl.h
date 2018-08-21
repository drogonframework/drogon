// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.


//taken from muduo and modified

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



#pragma once

#include <drogon/HttpResponse.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/net/InetAddress.h>
#include <map>

#include <string>
using std::string;
//#define CT_APPLICATION_JSON				1
//#define CT_TEXT_PLAIN					2
//#define CT_TEXT_HTML					3
//#define CT_APPLICATION_X_JAVASCRIPT		4
//#define CT_TEXT_CSS						5
//#define CT_TEXT_XML						6
//#define CT_APPLICATION_XML				7
//#define CT_TEXT_XSL						8
//#define CT_APPLICATION_OCTET_STREAM		9
//#define CT_APPLICATION_X_FONT_TRUETYPE	10
//#define CT_APPLICATION_X_FONT_OPENTYPE	11
//#define CT_APPLICATION_FONT_WOFF		12
//#define CT_APPLICATION_FONT_WOFF2		13
//#define CT_APPLICATION_VND_MS_FONTOBJ	14
//#define CT_IMAGE_SVG_XML				15
//#define CT_IMAGE_PNG					16
//#define CT_IMAGE_JPG					17
//#define CT_IMAGE_GIF					18
//#define CT_IMAGE_XICON					19
//#define CT_IMAGE_ICNS					20
//#define CT_IMAGE_BMP					21
#include <trantor/utils/MsgBuffer.h>
using namespace trantor;
namespace drogon
{
    class HttpResponseImpl:public HttpResponse
    {
        friend class HttpContext;
    public:

        explicit HttpResponseImpl()
                : _statusCode(kUnknown),
                  _closeConnection(false),
                  _left_body_length(0),
                  _current_chunk_length(0)
        {
        }
        virtual HttpStatusCode statusCode() override
        {
            return _statusCode;
        }
        virtual void setStatusCode(HttpStatusCode code) override
        {
            _statusCode = code;
            setStatusMessage(web_response_code_to_string(code));
        }

        virtual void setStatusCode(HttpStatusCode code, const std::string& status_message) override
        {
            _statusCode = code;
            setStatusMessage(status_message);
        }

        virtual void setVersion(const Version v) override
        {
            _v = v;
        }


        virtual void setCloseConnection(bool on) override
        {
            _closeConnection = on;
        }

        virtual bool closeConnection() const override
        {
            return _closeConnection;
        }

        virtual void setContentTypeCode(uint8_t type) override
        {
            _contentType=type;
            setContentType(web_content_type_to_string(type));
        }

//        virtual uint8_t contentTypeCode() override
//        {
//            return _contentType;
//        }

        virtual std::string getHeader(const std::string& key) const override
        {
            auto iter=_headers.find(key);
            if(iter == _headers.end())
            {
                return "";
            }
            else
            {
                return iter->second;
            }
        }
        virtual void addHeader(const std::string& key, const std::string& value) override
        {
            _headers[key] = value;
        }

        virtual void addHeader(const char* start, const char* colon, const char* end) override
        {
            std::string field(start, colon);
            ++colon;
            while (colon < end && isspace(*colon)) {
                ++colon;
            }
            std::string value(colon, end);
            while (!value.empty() && isspace(value[value.size() - 1])) {
                value.resize(value.size() - 1);
            }
            _headers[field] = value;
            transform(field.begin(), field.end(), field.begin(), ::tolower);
            if(field == "cookie") {
                //LOG_INFO<<"cookies!!!:"<<value;
                std::string::size_type pos;
                while((pos = value.find(";")) != std::string::npos) {
                    std::string coo = value.substr(0, pos);
                    auto epos = coo.find("=");
                    if(epos != std::string::npos) {
                        std::string cookie_name = coo.substr(0, epos);
                        std::string::size_type cpos=0;
                        while(cpos<cookie_name.length()&&isspace(cookie_name[cpos]))
                            cpos++;
                        cookie_name=cookie_name.substr(cpos);
                        std::string cookie_value = coo.substr(epos + 1);
                        _cookies.insert(std::make_pair(cookie_name,Cookie(cookie_name,cookie_value)));
                    }
                    value=value.substr(pos+1);
                }
                if(value.length()>0)
                {
                    std::string &coo = value;
                    auto epos = coo.find("=");
                    if(epos != std::string::npos) {
                        std::string cookie_name = coo.substr(0, epos);
                        std::string::size_type cpos=0;
                        while(cpos<cookie_name.length()&&isspace(cookie_name[cpos]))
                            cpos++;
                        cookie_name=cookie_name.substr(cpos);
                        std::string cookie_value = coo.substr(epos + 1);
                        _cookies.insert(std::make_pair(cookie_name,Cookie(cookie_name,cookie_value)));
                    }
                }
            }
        }

        virtual void addCookie(const std::string& key, const std::string& value) override
        {
            _cookies.insert(std::make_pair(key,Cookie(key,value)));
        }

        virtual void addCookie(const Cookie &cookie) override
        {
            _cookies.insert(std::make_pair(cookie.key(),cookie));
        }

        virtual void setBody(const std::string& body) override
        {
            _body = body;
        }
        virtual void setBody(std::string&& body) override
        {
            _body = std::move(body);
        }

        virtual void redirect(const std::string& url) override
        {
            _headers["Location"] = url;
        }
        void appendToBuffer(MsgBuffer* output) const;

        virtual void clear() override
        {
            _statusCode = kUnknown;
            _v = kHttp11;
            _statusMessage.clear();
            _headers.clear();
            _cookies.clear();
            _body.clear();
            _left_body_length = 0;
            _current_chunk_length = 0;
        }

//	void setReceiveTime(trantor::Date t)
//    {
//        receiveTime_ = t;
//    }

        virtual std::string getBody() const override
        {
            return _body;
        }

        void swap(HttpResponseImpl &that)
        {
            _headers.swap(that._headers);
            _cookies.swap(that._cookies);
            std::swap(_statusCode,that._statusCode);
            std::swap(_v,that._v);
            _statusMessage.swap(that._statusMessage);
            std::swap(_closeConnection,that._closeConnection);
            _body.swap(that._body);
            std::swap(_left_body_length,that._left_body_length);
            std::swap(_current_chunk_length,that._current_chunk_length);
            std::swap(_contentType,that._contentType);
            _jsonPtr.swap(that._jsonPtr);
        }
        void parseJson() const
        {
            //parse json data in reponse
            _jsonPtr=std::make_shared<Json::Value>();
            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;
            JSONCPP_STRING errs;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            if (!reader->parse(_body.data(), _body.data() + _body.size(), _jsonPtr.get() , &errs))
            {
                LOG_ERROR<<errs;
                _jsonPtr.reset();
            }
        }
        virtual const std::shared_ptr<Json::Value> getJsonObject() const override
        {
            if(!_jsonPtr){
                parseJson();
            }
            return _jsonPtr;
        }
    protected:
        static const std::string web_content_type_to_string(uint8_t contenttype);
        static const std::string web_response_code_to_string(int code);

    private:
        std::map<std::string, std::string> _headers;
        std::map<std::string, Cookie> _cookies;
        HttpStatusCode _statusCode;
        // FIXME: add http version
        Version _v;
        std::string _statusMessage;
        bool _closeConnection;
        std::string _body;
        size_t _left_body_length;
        size_t _current_chunk_length;
        uint8_t _contentType=CT_TEXT_HTML;

        mutable std::shared_ptr<Json::Value> _jsonPtr;
        //trantor::Date receiveTime_;

        void setContentType(const std::string& contentType)
        {
            addHeader("Content-Type", contentType);
        }
        void setStatusMessage(const std::string& message)
        {
            _statusMessage = message;
        }
    };

}


