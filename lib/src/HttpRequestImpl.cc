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
#include "HttpAppFrameworkImpl.h"

#include <drogon/utils/Utilities.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace drogon;

void HttpRequestImpl::parseParameters() const
{
    auto input = queryView();
    if (input.empty())
        return;
    std::string type = getHeaderBy("content-type");
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (_method == Get ||
        (_method == Post &&
         (type.empty() ||
          type.find("application/x-www-form-urlencoded") != std::string::npos)))
    {
        string_view::size_type pos = 0;
        while ((input[pos] == '?' || isspace(input[pos])) &&
               pos < input.length())
        {
            pos++;
        }
        auto value = input.substr(pos);
        while ((pos = value.find("&")) != string_view::npos)
        {
            auto coo = value.substr(0, pos);
            auto epos = coo.find("=");
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            auto &coo = value;
            auto epos = coo.find("=");
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
        }
    }
    if (type.find("application/json") != std::string::npos)
    {
        // parse json data in request
        _jsonPtr = std::make_shared<Json::Value>();
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(input.data(),
                           input.data() + input.size(),
                           _jsonPtr.get(),
                           &errs))
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

void HttpRequestImpl::appendToBuffer(trantor::MsgBuffer *output) const
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
        case Options:
            output->append("OPTIONS ");
            break;
        default:
            return;
    }

    if (!_path.empty())
    {
        output->append(utils::urlEncode(_path));
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
        content = utils::urlEncode(content);
        if (_method == Get || _method == Delete || _method == Head)
        {
            auto ret = std::find(output->peek(),
                                 (const char *)output->beginWrite(),
                                 '?');
            if (ret != output->beginWrite())
            {
                if (ret != output->beginWrite() - 1)
                {
                    output->append("&");
                }
            }
            else
            {
                output->append("?");
            }
            output->append(content);
            content.clear();
        }
        else if (_contentType == CT_APPLICATION_JSON)
        {
            /// Can't set parameters in content in this case
            LOG_ERROR
                << "You can't set parameters in the query string when the "
                   "request content type is JSON and http method "
                   "is POST or PUT";
            LOG_ERROR
                << "Please put these parameters into the path or into the json "
                   "string";
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
        if (mReq)
        {
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
                    pbuf->pubseekoff(0, infile.beg);  // rewind
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
    }
    assert(!(!content.empty() && !_content.empty()));
    if (!content.empty() || !_content.empty())
    {
        char buf[64];
        auto len = snprintf(buf,
                            sizeof(buf),
                            "Content-Length: %lu\r\n",
                            static_cast<long unsigned int>(content.length() +
                                                           _content.length()));
        output->append(buf, len);
        if (_contentTypeString.empty())
        {
            auto &type = webContentTypeToString(_contentType);
            output->append(type.data(), type.length());
        }
    }
    if (!_contentTypeString.empty())
    {
        output->append(_contentTypeString);
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
        output->unwrite(1);  // delete last ';'
        output->append("\r\n");
    }

    output->append("\r\n");
    if (!content.empty())
        output->append(content);
    if (!_content.empty())
        output->append(_content);
}

void HttpRequestImpl::addHeader(const char *start,
                                const char *colon,
                                const char *end)
{
    std::string field(start, colon);
    // Field name is case-insensitive.so we transform it to lower;(rfc2616-4.2)
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
                while (cpos < cookie_name.length() &&
                       isspace(cookie_name[cpos]))
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
                while (cpos < cookie_name.length() &&
                       isspace(cookie_name[cpos]))
                    cpos++;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                _cookies[std::move(cookie_name)] = std::move(cookie_value);
            }
        }
    }
    else
    {
        if (field == "content-length")
        {
            _contentLen = std::stoull(value.c_str());
        }
        else if (field == "expect")
        {
            _expect = value;
        }
        _headers.emplace(std::move(field), std::move(value));
    }
}

HttpRequestPtr HttpRequest::newHttpRequest()
{
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Get);
    req->setVersion(drogon::HttpRequest::kHttp11);
    return req;
}

HttpRequestPtr HttpRequest::newHttpFormPostRequest()
{
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Post);
    req->setVersion(drogon::HttpRequest::kHttp11);
    req->_contentType = CT_APPLICATION_X_FORM;
    return req;
}

HttpRequestPtr HttpRequest::newHttpJsonRequest(const Json::Value &data)
{
    static std::once_flag once;
    static Json::StreamWriterBuilder builder;
    std::call_once(once, []() {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
    });
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Get);
    req->setVersion(drogon::HttpRequest::kHttp11);
    req->_contentType = CT_APPLICATION_JSON;
    req->setContent(writeString(builder, data));
    return req;
}

HttpRequestPtr HttpRequest::newFileUploadRequest(
    const std::vector<UploadFile> &files)
{
    return std::make_shared<HttpFileUploadRequest>(files);
}

void HttpRequestImpl::swap(HttpRequestImpl &that) noexcept
{
    std::swap(_method, that._method);
    std::swap(_version, that._version);
    _path.swap(that._path);
    _query.swap(that._query);

    _headers.swap(that._headers);
    _cookies.swap(that._cookies);
    _parameters.swap(that._parameters);
    _jsonPtr.swap(that._jsonPtr);
    _sessionPtr.swap(that._sessionPtr);

    std::swap(_peer, that._peer);
    std::swap(_local, that._local);
    _date.swap(that._date);
    _content.swap(that._content);
    std::swap(_contentLen, that._contentLen);
}

const char *HttpRequestImpl::methodString() const
{
    const char *result = "UNKNOWN";
    switch (_method)
    {
        case Get:
            result = "GET";
            break;
        case Post:
            result = "POST";
            break;
        case Head:
            result = "HEAD";
            break;
        case Put:
            result = "PUT";
            break;
        case Delete:
            result = "DELETE";
            break;
        case Options:
            result = "OPTIONS";
            break;
        default:
            break;
    }
    return result;
}

bool HttpRequestImpl::setMethod(const char *start, const char *end)
{
    assert(_method == Invalid);
    string_view m(start, end - start);
    switch (m.length())
    {
        case 3:
            if (m == "GET")
            {
                _method = Get;
            }
            else if (m == "PUT")
            {
                _method = Put;
            }
            else
            {
                _method = Invalid;
            }
            break;
        case 4:
            if (m == "POST")
            {
                _method = Post;
            }
            else if (m == "HEAD")
            {
                _method = Head;
            }
            else
            {
                _method = Invalid;
            }
            break;
        case 6:
            if (m == "DELETE")
            {
                _method = Delete;
            }
            else
            {
                _method = Invalid;
            }
            break;
        case 7:
            if (m == "OPTIONS")
            {
                _method = Options;
            }
            else
            {
                _method = Invalid;
            }
            break;
        default:
            _method = Invalid;
            break;
    }

    // if (_method != Invalid)
    // {
    //     _content = "";
    //     _query = "";
    //     _cookies.clear();
    //     _parameters.clear();
    //     _headers.clear();
    // }
    return _method != Invalid;
}

HttpRequestImpl::~HttpRequestImpl()
{
}

void HttpRequestImpl::reserveBodySize()
{
    if (_contentLen <=
        HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize())
    {
        _content.reserve(_contentLen);
    }
    else
    {
        // Store data of body to a temperary file
        auto tmpfile = HttpAppFrameworkImpl::instance().getUploadPath();
        auto fileName = utils::getUuid();
        tmpfile.append("/tmp/")
            .append(1, fileName[0])
            .append(1, fileName[1])
            .append("/")
            .append(fileName);
        _cacheFilePtr = std::make_unique<CacheFile>(tmpfile);
    }
}