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


#include <trantor/utils/MsgBuffer.h>
#include <map>

#include <string>
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
using namespace trantor;
class HttpResponse
{
	friend class HttpContext;
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

    explicit HttpResponse(bool close)
            : statusCode_(kUnknown),
              closeConnection_(close),
			left_body_length_(0),
			current_chunk_length_(0)
    {
    }

	explicit HttpResponse()
            : statusCode_(kUnknown),
              closeConnection_(false),
			left_body_length_(0)
    {
    }

    void setStatusCode(HttpStatusCode code)
    {
        statusCode_ = code;
        setStatusMessage(web_response_code_to_string(code));
    }

	void setStatusCode(HttpStatusCode code, const std::string& status_message)
    {
        statusCode_ = code;
        setStatusMessage(status_message);
    }

	void setVersion(const Version v)
    {
        v_ = v;
    }


    void setCloseConnection(bool on)
    {
        closeConnection_ = on;
    }

    bool closeConnection() const
    {
        return closeConnection_;
    }

    void setContentTypeCode(uint8_t type)
    {
        contentType_=type;
        setContentType(web_content_type_to_string(type));
    }


    std::string getHeader(const std::string& key)
	{
		if(headers_.find(key) == headers_.end())
		{
			return "";
		}
		else
		{
			return headers_[key];
		}
	}
    void addHeader(const std::string& key, const std::string& value)
    {
        headers_[key] = value;
    }

	void addHeader(const char* start, const char* colon, const char* end)
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
        headers_[field] = value;
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
                    while(isspace(cookie_name[cpos])&&cpos<cookie_name.length())
                        cpos++;
                    cookie_name=cookie_name.substr(cpos);
                    std::string cookie_value = coo.substr(epos + 1);
                    cookies_[cookie_name] = cookie_value;
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
                    while(isspace(cookie_name[cpos])&&cpos<cookie_name.length())
                        cpos++;
                    cookie_name=cookie_name.substr(cpos);
                    std::string cookie_value = coo.substr(epos + 1);
                    cookies_[cookie_name] = cookie_value;
                }
            }
        }
    }

    void addCookie(const std::string& key, const std::string& value)
    {
        cookies_[key] = value;
    }
    void setBody(const std::string& body)
    {
        body_ = body;
    }
    void redirect(const std::string& url)
    {
        headers_["Location"] = url;
    }
    void appendToBuffer(MsgBuffer* output) const;

	void clear()
    {
        statusCode_ = kUnknown;
		v_ = kHttp11;
        statusMessage_.clear();
        headers_.clear();
        cookies_.clear();
		body_.clear();
        left_body_length_ = 0;
		current_chunk_length_ = 0;
    }

//	void setReceiveTime(trantor::Date t)
//    {
//        receiveTime_ = t;
//    }

	std::string getBody() const
	{
		return body_;
	}

protected:
    static const std::string web_content_type_to_string(uint8_t contenttype);
    static const std::string web_response_code_to_string(int code);

private:
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> cookies_;
    HttpStatusCode statusCode_;
    // FIXME: add http version
	Version v_;
    std::string statusMessage_;
    bool closeConnection_;
    std::string body_;
	size_t left_body_length_;
	size_t current_chunk_length_;
    uint8_t contentType_;
	//trantor::Date receiveTime_;

    void setContentType(const std::string& contentType)
    {
        addHeader("Content-Type", contentType);
    }
    void setStatusMessage(const std::string& message)
    {
        statusMessage_ = message;
    }
};


