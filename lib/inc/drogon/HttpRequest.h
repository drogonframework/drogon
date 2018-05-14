// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

#include <drogon/Session.h>
#include <map>
#include <string>

namespace drogon
{
    /*
     * abstract class for webapp developer to get Http client request;
     * */
    class HttpRequest
    {
    public:
        enum Version {
            kUnknown, kHttp10, kHttp11
        };
        enum Method {
            kInvalid, kGet, kPost, kHead, kPut, kDelete
        };
        virtual const char* methodString() const=0;
        virtual Method method() const=0;
        virtual std::string getHeader(const std::string& field) const=0;
        virtual std::string getCookie(const std::string& field) const=0;
        virtual const std::map<std::string, std::string>& headers() const=0;
        virtual const std::map<std::string, std::string>& cookies() const=0;
        virtual const std::string& query() const=0;
        virtual const std::string& path() const=0;
        virtual Version getVersion() const=0;
        virtual SessionPtr session() const=0;
        virtual ~HttpRequest(){}
    };
}