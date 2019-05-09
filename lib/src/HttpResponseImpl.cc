/**
 *
 *  HttpResponseImpl.cc
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

#include "HttpAppFrameworkImpl.h"
#include "HttpResponseImpl.h"
#include "HttpUtils.h"
#include <drogon/HttpViewBase.h>
#include <drogon/HttpViewData.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <fstream>
#include <stdio.h>
#include <sys/stat.h>

using namespace trantor;
using namespace drogon;

HttpResponsePtr HttpResponse::newHttpResponse()
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_TEXT_HTML);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(const Json::Value &data)
{
    static std::once_flag once;
    static Json::StreamWriterBuilder builder;
    std::call_once(once, []() {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
    });
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_APPLICATION_JSON);
    res->setBody(writeString(builder, data));
    return res;
}

HttpResponsePtr HttpResponse::newNotFoundResponse()
{
    auto &resp = HttpAppFrameworkImpl::instance().getCustom404Page();
    if (resp)
    {
        return resp;
    }
    else
    {
        static std::once_flag once;
        static HttpResponsePtr notFoundResp;
        std::call_once(once, []() {
            HttpViewData data;
            data.insert("version", getVersion());
            notFoundResp = HttpResponse::newHttpViewResponse("NotFound", data);
            notFoundResp->setStatusCode(k404NotFound);
        });
        return notFoundResp;
    }
}
HttpResponsePtr HttpResponse::newRedirectionResponse(const std::string &location)
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(k302Found);
    res->redirect(location);
    return res;
}

HttpResponsePtr HttpResponse::newHttpViewResponse(const std::string &viewName, const HttpViewData &data)
{
    return HttpViewBase::genHttpResponse(viewName, data);
}

HttpResponsePtr HttpResponse::newFileResponse(const std::string &fullPath, const std::string &attachmentFileName, ContentType type)
{
    
    std::ifstream infile(fullPath, std::ifstream::binary);
    LOG_TRACE << "send http file:" << fullPath;
    if (!infile)
    {
        auto resp = HttpResponse::newNotFoundResponse();
        return resp;
    }
    auto resp = std::make_shared<HttpResponseImpl>();
    std::streambuf *pbuf = infile.rdbuf();
    std::streamsize filesize = pbuf->pubseekoff(0, infile.end);
    pbuf->pubseekoff(0, infile.beg); // rewind
    if (HttpAppFrameworkImpl::instance().useSendfile() &&
        filesize > 1024 * 200)
    //TODO : Is 200k an appropriate value? Or set it to be configurable
    {
        //The advantages of sendfile() can only be reflected in sending large files.
        resp->setSendfile(fullPath);
    }
    else
    {
        std::string str;
        str.resize(filesize);
        pbuf->sgetn(&str[0], filesize);
        resp->setBody(std::move(str));
    }
    resp->setStatusCode(k200OK);

    if (type == CT_NONE)
    {
        if (!attachmentFileName.empty())
        {
            resp->setContentTypeCode(drogon::getContentType(attachmentFileName));
        }
        else
        {
            resp->setContentTypeCode(drogon::getContentType(fullPath));
        }
    }
    else
    {
        resp->setContentTypeCode(type);
    }

    if (!attachmentFileName.empty())
    {
        resp->addHeader("Content-Disposition", "attachment; filename=" + attachmentFileName);
    }

    return resp;
}

void HttpResponseImpl::makeHeaderString(const std::shared_ptr<std::string> &headerStringPtr) const
{
    char buf[128];
    assert(headerStringPtr);
    auto len = snprintf(buf, sizeof buf, "HTTP/1.1 %d ", _statusCode);
    headerStringPtr->append(buf, len);
    if (!_statusMessage.empty())
        headerStringPtr->append(_statusMessage.data(), _statusMessage.length());
    headerStringPtr->append("\r\n");
    if (_sendfileName.empty())
    {
        len = snprintf(buf, sizeof buf, "Content-Length: %lu\r\n", static_cast<long unsigned int>(_bodyPtr->size()));
    }
    else
    {
        struct stat filestat;
        if (stat(_sendfileName.c_str(), &filestat) < 0)
        {
            LOG_SYSERR << _sendfileName << " stat error";
            return;
        }
        len = snprintf(buf, sizeof buf, "Content-Length: %llu\r\n", static_cast<long long unsigned int>(filestat.st_size));
    }

    headerStringPtr->append(buf, len);
    if (_headers.find("Connection") == _headers.end())
    {
        if (_closeConnection)
        {
            headerStringPtr->append("Connection: close\r\n");
        }
        else
        {

            //output->append("Connection: Keep-Alive\r\n");
        }
    }
    headerStringPtr->append(_contentTypeString.data(), _contentTypeString.length());
    for (auto it = _headers.begin(); it != _headers.end(); ++it)
    {
        headerStringPtr->append(it->first);
        headerStringPtr->append(": ");
        headerStringPtr->append(it->second);
        headerStringPtr->append("\r\n");
    }
    headerStringPtr->append(HttpAppFrameworkImpl::instance().getServerHeaderString());
}

std::shared_ptr<std::string> HttpResponseImpl::renderToString() const
{
    if (_expriedTime >= 0)
    {
        if (_datePos != std::string::npos)
        {
            auto now = trantor::Date::now();
            bool isDateChanged = ((now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC) != _httpStringDate);
            assert(_httpString);
            if (isDateChanged)
            {
                _httpStringDate = now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
                auto newDate = utils::getHttpFullDate(now);

                _httpString = std::make_shared<std::string>(*_httpString);
                memcpy((void *)&(*_httpString)[_datePos], newDate, strlen(newDate));
                return _httpString;
            }

            return _httpString;
        }
    }
    auto httpString = std::make_shared<std::string>();
    httpString->reserve(256);
    if (!_fullHeaderString)
    {
        makeHeaderString(httpString);
    }
    else
    {
        httpString->append(*_fullHeaderString);
    }

    //output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {

            httpString->append(it->second.cookieString());
        }
    }

    //output Date header
    httpString->append("Date: ");
    auto datePos = httpString->length();
    httpString->append(utils::getHttpFullDate(trantor::Date::date()));
    httpString->append("\r\n\r\n");

    LOG_TRACE << "reponse(no body):" << httpString->c_str();
    httpString->append(*_bodyPtr);
    if (_expriedTime >= 0)
    {
        _datePos = datePos;
        _httpString = httpString;
    }
    return httpString;
}

std::shared_ptr<std::string> HttpResponseImpl::renderHeaderForHeadMethod() const
{
    auto httpString = std::make_shared<std::string>();
    httpString->reserve(256);
    if (!_fullHeaderString)
    {
        makeHeaderString(httpString);
    }
    else
    {
        httpString->append(*_fullHeaderString);
    }

    //output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {

            httpString->append(it->second.cookieString());
        }
    }

    //output Date header
    httpString->append("Date: ");
    httpString->append(utils::getHttpFullDate(trantor::Date::date()));
    httpString->append("\r\n\r\n");

    return httpString;
}
