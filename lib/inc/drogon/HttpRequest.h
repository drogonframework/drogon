/**
 *
 *  HttpRequest.h
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

#pragma once

#include <drogon/Session.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <json/json.h>
#include <map>
#include <string>
#include <memory>
namespace drogon
{
class HttpRequest;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
enum HttpMethod
{
    Get = 0,
    Post,
    Head,
    Put,
    Delete,
    Invalid
};
/*
     * abstract class for webapp developer to get Http client request;
     * */
class HttpRequest
{
  public:
    enum Version
    {
        kUnknown = 0,
        kHttp10 = 1,
        kHttp11 = 2
    };

    virtual const char *methodString() const = 0;
    virtual HttpMethod method() const = 0;
    virtual std::string getHeader(const std::string &field) const = 0;
    virtual std::string getCookie(const std::string &field) const = 0;
    virtual const std::map<std::string, std::string> &headers() const = 0;
    virtual const std::map<std::string, std::string> &cookies() const = 0;
    virtual const std::string &query() const = 0;
    virtual const std::string &path() const = 0;
    virtual Version getVersion() const = 0;
    virtual SessionPtr session() const = 0;
    virtual const std::map<std::string, std::string> &getParameters() const = 0;
    virtual const trantor::InetAddress &peerAddr() const = 0;
    virtual const trantor::InetAddress &localAddr() const = 0;
    virtual const trantor::Date &receiveDate() const = 0;
    virtual ~HttpRequest() {}
    virtual const std::shared_ptr<Json::Value> getJsonObject() const = 0;

    virtual void setMethod(const HttpMethod method) = 0;
    virtual void setPath(const std::string &path) = 0;
    virtual void setParameter(const std::string &key, const std::string &value) = 0;
    static HttpRequestPtr newHttpRequest();
};

} // namespace drogon
