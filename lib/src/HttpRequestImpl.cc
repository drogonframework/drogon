/**
 *
 *  HttpRequestImpl.cc
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

#include "HttpRequestImpl.h"
#include "HttpFileUploadRequest.h"
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <fstream>
#include <unistd.h>

using namespace drogon;

void HttpRequestImpl::parseParameter()
{
    const std::string &input = query();
    if (input.empty())
        return;
    std::string type = getHeaderBy("content-type");
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (_method == Get || (_method == Post && (type == "" || type.find("application/x-www-form-urlencoded") != std::string::npos)))
    {

        std::string::size_type pos = 0;
        while ((input[pos] == '?' || isspace(input[pos])) && pos < input.length())
        {
            pos++;
        }
        std::string value = input.substr(pos);
        while ((pos = value.find("&")) != std::string::npos)
        {
            std::string coo = value.substr(0, pos);
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode = urlDecode(pvalue);
                std::string keydecode = urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            std::string &coo = value;
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode = urlDecode(pvalue);
                std::string keydecode = urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
        }
    }
    if (type.find("application/json") != std::string::npos)
    {
        //parse json data in request
        _jsonPtr = std::make_shared<Json::Value>();
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(input.data(), input.data() + input.size(), _jsonPtr.get(), &errs))
        {
            LOG_ERROR << errs;
            _jsonPtr.reset();
        }
    }
    // LOG_TRACE << "_parameters:";
    // for (auto iter : _parameters)
    // {
    //     LOG_TRACE << iter.first << "=" << iter.second;
    // }
}

void HttpRequestImpl::appendToBuffer(MsgBuffer *output) const
{
    switch (_method)
    {
    case Get:
        output->append("GET ");
        break;
    case Post:
        output->append("POST ");
        break;
    case Head:
        output->append("HEAD ");
        break;
    case Put:
        output->append("PUT ");
        break;
    case Delete:
        output->append("DELETE ");
        break;
    default:
        return;
    }

    if (!_path.empty())
    {
        output->append(_path);
    }
    else
    {
        output->append("/");
    }

    std::string content;
    if (!_parameters.empty() && _contentType != CT_MULTIPART_FORM_DATA)
    {
        for (auto const &p : _parameters)
        {
            content.append(p.first);
            content.append("=");
            content.append(p.second);
            content.append("&");
        }
        content.resize(content.length() - 1);
        content = urlEncode(content);
        if (_method == Get || _method == Delete)
        {
            output->append("?");
            output->append(content);
            content.clear();
        }
        else if (_contentType == CT_APPLICATION_JSON)
        {
            ///Can't set parameters in content in this case
            LOG_ERROR << "You can't set parameters in the query string when the request content type is JSON and http method is POST or PUT";
            LOG_ERROR << "Please put these parameters into the path or into the json string";
            content.clear();
        }
    }

    output->append(" ");
    if (_version == kHttp11)
    {
        output->append("HTTP/1.1");
    }
    else if (_version == kHttp10)
    {
        output->append("HTTP/1.0");
    }
    else
    {
        return;
    }
    output->append("\r\n");
    if (_contentType == CT_MULTIPART_FORM_DATA)
    {
        auto mReq = dynamic_cast<const HttpFileUploadRequest *>(this);
        assert(mReq);
        for (auto &param : mReq->getParameters())
        {
            content.append("--");
            content.append(mReq->boundary());
            content.append("\r\n");
            content.append("Content-Disposition: form-data; name=\"");
            content.append(param.first);
            content.append("\"\r\n\r\n");
            content.append(param.second);
            content.append("\r\n");
        }
        for (auto &file : mReq->files())
        {
            content.append("--");
            content.append(mReq->boundary());
            content.append("\r\n");
            content.append("Content-Disposition: form-data; name=\"");
            content.append(file.itemName());
            content.append("\"; filename=\"");
            content.append(file.fileName());
            content.append("\"\r\n\r\n");
            std::ifstream infile(file.path(), std::ifstream::binary);
            if (!infile)
            {
                LOG_ERROR << file.path() << " not found";
            }
            else
            {
                std::streambuf *pbuf = infile.rdbuf();
                std::streamsize filesize = pbuf->pubseekoff(0, infile.end);
                pbuf->pubseekoff(0, infile.beg); // rewind
                std::string str;
                str.resize(filesize);
                pbuf->sgetn(&str[0], filesize);
                content.append(std::move(str));
            }
            content.append("\r\n");
        }
        content.append("--");
        content.append(mReq->boundary());
        content.append("--");
    }
    assert(!(!content.empty() && !_content.empty()));
    if (!content.empty() || !_content.empty())
    {
        char buf[64];
        snprintf(buf, sizeof buf, "Content-Length: %lu\r\n", static_cast<long unsigned int>(content.length() + _content.length()));
        output->append(buf);
        if (_headers.find("Content-Type") == _headers.end())
        {
            output->append("Content-Type: ");
            output->append(webContentTypeToString(_contentType));
            output->append("\r\n");
        }
    }

    for (auto it = _headers.begin(); it != _headers.end(); ++it)
    {
        output->append(it->first);
        output->append(": ");
        output->append(it->second);
        output->append("\r\n");
    }
    if (_cookies.size() > 0)
    {
        output->append("Cookie: ");
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {
            output->append(it->first);
            output->append("= ");
            output->append(it->second);
            output->append(";");
        }
        output->unwrite(1); //delete last ';'
        output->append("\r\n");
    }

    output->append("\r\n");
    if (!content.empty())
        output->append(content);
    if (!_content.empty())
        output->append(_content);
}

void HttpRequestImpl::addHeader(const char *start, const char *colon, const char *end)
{
    std::string field(start, colon);
    //Field name is case-insensitive.so we transform it to lower;(rfc2616-4.2)
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);
    ++colon;
    while (colon < end && isspace(*colon))
    {
        ++colon;
    }
    std::string value(colon, end);
    while (!value.empty() && isspace(value[value.size() - 1]))
    {
        value.resize(value.size() - 1);
    }

    if (field == "cookie")
    {
        LOG_TRACE << "cookies!!!:" << value;
        std::string::size_type pos;
        while ((pos = value.find(";")) != std::string::npos)
        {
            std::string coo = value.substr(0, pos);
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string cookie_name = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < cookie_name.length() && isspace(cookie_name[cpos]))
                    cpos++;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                _cookies[std::move(cookie_name)] = std::move(cookie_value);
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            std::string &coo = value;
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string cookie_name = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < cookie_name.length() && isspace(cookie_name[cpos]))
                    cpos++;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                _cookies[std::move(cookie_name)] = std::move(cookie_value);
            }
        }
    }
    else
    {
        _headers[std::move(field)] = std::move(value);
    }
}

HttpRequestPtr HttpRequest::newHttpRequest()
{
    auto req = std::make_shared<HttpRequestImpl>();
    req->setMethod(drogon::Get);
    req->setVersion(drogon::HttpRequest::kHttp11);
    return req;
}

HttpRequestPtr HttpRequest::newHttpFormPostRequest()
{
    auto req = std::make_shared<HttpRequestImpl>();
    req->setMethod(drogon::Post);
    req->setVersion(drogon::HttpRequest::kHttp11);
    req->_contentType = CT_APPLICATION_X_FORM;
    return req;
}

HttpRequestPtr HttpRequest::newHttpJsonRequest(const Json::Value &data)
{
    auto req = std::make_shared<HttpRequestImpl>();
    req->setMethod(drogon::Get);
    req->setVersion(drogon::HttpRequest::kHttp11);
    req->_contentType = CT_APPLICATION_JSON;
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    req->setContent(writeString(builder, data));
    return req;
}

HttpRequestPtr HttpRequest::newFileUploadRequest(const std::vector<UploadFile> &files)
{
    return std::make_shared<HttpFileUploadRequest>(files);
}
