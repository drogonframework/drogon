// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

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

#include "HttpRequestImpl.h"
#include "Utilities.h"

#include <iostream>
using namespace drogon;

void HttpRequestImpl::parsePremeter()
{
    const std::string &type=getHeader("Content-Type");
    const std::string &input=query();
    if(method_==kGet||(method_==kPost&&(type==""||type.find("application/x-www-form-urlencoded")!=std::string::npos)))
    {

        std::string::size_type pos=0;
        while((input[pos]=='?'||isspace(input[pos]))&&pos<input.length())
        {
            pos++;
        }
        std::string value=input.substr(pos);
        while((pos = value.find("&")) != std::string::npos) {
            std::string coo = value.substr(0, pos);
            auto epos = coo.find("=");
            if(epos != std::string::npos) {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos=0;
                while(cpos<key.length()&&isspace(key[cpos]))
                    cpos++;
                key=key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode=urlDecode(pvalue);
                std::string keydecode=urlDecode(key);
                premeter_[keydecode] = pdecode;
            }
            value=value.substr(pos+1);
        }
        if(value.length()>0)
        {
            std::string &coo = value;
            auto epos = coo.find("=");
            if(epos != std::string::npos) {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos=0;
                while(cpos<key.length()&&isspace(key[cpos]))
                    cpos++;
                key=key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode=urlDecode(pvalue);
                std::string keydecode=urlDecode(key);
                premeter_[keydecode] = pdecode;
            }
        }
    }
    if(type.find("application/json")!=std::string::npos)
    {
        //parse json data in request
        _jsonPtr=std::make_shared<Json::Value>();
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(input.data(), input.data() + input.size(), _jsonPtr.get() , &errs))
        {
            LOG_ERROR<<errs;
            _jsonPtr.reset();
        }

    }
    LOG_DEBUG<<"premeter:";
    for(auto iter:premeter_)
    {
        LOG_DEBUG<<iter.first<<"="<<iter.second;
    }

}

void HttpRequestImpl::appendToBuffer(MsgBuffer* output) const
{
	switch(method_)
	{
	case kDelete:
		output->append("DELETE ");
		break;
	case kGet:
		output->append("GET ");
		break;
	case kHead:
		output->append("HEAD ");
		break;
	case kPost:
		output->append("POST ");
		break;
	case kPut:
		output->append("PUT ");
		break;
	default:
		return;
	}

	if(path_.size() != 0)
	{
		output->append(path_);
		output->append(" ");
	}
	else
	{
		output->append("/ ");
	}

	if(version_ == kHttp11)
	{
		output->append("HTTP/1.1");
	}
	else if(version_ == kHttp10)
	{
		output->append("HTTP/1.0");
	}
	else
	{
		return;
	}
    output->append("\r\n");

    for (std::map<std::string, std::string>::const_iterator it = headers_.begin();
         it != headers_.end();
         ++it) {
        output->append(it->first);
        output->append(": ");
        output->append(it->second);
        output->append("\r\n");
    }
    if(cookies_.size() > 0) {
        output->append("Set-Cookie: ");
        for(auto it = cookies_.begin(); it != cookies_.end(); it++) {
            output->append(it->first);
            output->append("= ");
            output->append(it->second);
            output->append(";");
        }
        output->unwrite(1);//delete last ';'
        output->append("\r\n");
    }

    output->append("\r\n");

	//LOG_INFO<<"request(no body):"<<output->peek();
	output->append(content_);
}
