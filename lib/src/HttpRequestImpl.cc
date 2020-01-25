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
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace drogon;
void HttpRequestImpl::parseJson() const
{
    auto input = contentView();
    if (input.empty())
        return;
    std::string type = getHeaderBy("content-type");
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (type.find("application/json") != std::string::npos)
    {
        static std::once_flag once;
        static Json::CharReaderBuilder builder;
        std::call_once(once, []() { builder["collectComments"] = false; });
        jsonPtr_ = std::make_shared<Json::Value>();
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(input.data(),
                           input.data() + input.size(),
                           jsonPtr_.get(),
                           &errs))
        {
            LOG_ERROR << errs;
            jsonPtr_.reset();
        }
    }
}
void HttpRequestImpl::parseParameters() const
{
    auto input = queryView();
    if (!input.empty())
    {
        string_view::size_type pos = 0;
        while ((input[pos] == '?' || isspace(input[pos])) &&
               pos < input.length())
        {
            ++pos;
        }
        auto value = input.substr(pos);
        while ((pos = value.find('&')) != string_view::npos)
        {
            auto coo = value.substr(0, pos);
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            auto &coo = value;
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
        }
    }

    input = contentView();
    if (input.empty())
        return;
    std::string type = getHeaderBy("content-type");
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (type.empty() ||
        type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        string_view::size_type pos = 0;
        while ((input[pos] == '?' || isspace(input[pos])) &&
               pos < input.length())
        {
            ++pos;
        }
        auto value = input.substr(pos);
        while ((pos = value.find('&')) != string_view::npos)
        {
            auto coo = value.substr(0, pos);
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            auto &coo = value;
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
        }
    }
}

void HttpRequestImpl::appendToBuffer(trantor::MsgBuffer *output) const
{
    switch (method_)
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

    if (!path_.empty())
    {
        output->append(utils::urlEncode(path_));
    }
    else
    {
        output->append("/");
    }

    std::string content;

    if (!passThrough_ &&
        (!parameters_.empty() && contentType_ != CT_MULTIPART_FORM_DATA))
    {
        for (auto const &p : parameters_)
        {
            content.append(utils::urlEncodeComponent(p.first));
            content.append("=");
            content.append(utils::urlEncodeComponent(p.second));
            content.append("&");
        }
        content.resize(content.length() - 1);
        if (method_ == Get || method_ == Delete || method_ == Head)
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
        else if (contentType_ == CT_APPLICATION_JSON)
        {
            /// Can't set parameters in content in this case
            LOG_ERROR
                << "You can't set parameters in the query string when the "
                   "request content type is JSON and http method "
                   "is POST or PUT";
            LOG_ERROR << "Please put these parameters into the path or "
                         "into the json "
                         "string";
            content.clear();
        }
    }

    output->append(" ");
    if (version_ == Version::kHttp11)
    {
        output->append("HTTP/1.1");
    }
    else if (version_ == Version::kHttp10)
    {
        output->append("HTTP/1.0");
    }
    else
    {
        return;
    }
    output->append("\r\n");

    if (!passThrough_ && contentType_ == CT_MULTIPART_FORM_DATA)
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
    assert(!(!content.empty() && !content_.empty()));
    if (!passThrough_ && (!content.empty() || !content_.empty()))
    {
        char buf[64];
        auto len = snprintf(buf,
                            sizeof(buf),
                            "Content-Length: %lu\r\n",
                            static_cast<long unsigned int>(content.length() +
                                                           content_.length()));
        output->append(buf, len);
        if (contentTypeString_.empty())
        {
            auto &type = webContentTypeToString(contentType_);
            output->append(type.data(), type.length());
        }
    }
    if (!passThrough_ && !contentTypeString_.empty())
    {
        output->append(contentTypeString_);
    }
    for (auto it = headers_.begin(); it != headers_.end(); ++it)
    {
        output->append(it->first);
        output->append(": ");
        output->append(it->second);
        output->append("\r\n");
    }
    if (cookies_.size() > 0)
    {
        output->append("Cookie: ");
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it)
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
    if (!content_.empty())
        output->append(content_);
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
    if (field.length() == 6 && field == "cookie")
    {
        LOG_TRACE << "cookies!!!:" << value;
        std::string::size_type pos;
        while ((pos = value.find(';')) != std::string::npos)
        {
            std::string coo = value.substr(0, pos);
            auto epos = coo.find('=');
            if (epos != std::string::npos)
            {
                std::string cookie_name = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < cookie_name.length() &&
                       isspace(cookie_name[cpos]))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                cookies_[std::move(cookie_name)] = std::move(cookie_value);
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            std::string &coo = value;
            auto epos = coo.find('=');
            if (epos != std::string::npos)
            {
                std::string cookie_name = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < cookie_name.length() &&
                       isspace(cookie_name[cpos]))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                cookies_[std::move(cookie_name)] = std::move(cookie_value);
            }
        }
    }
    else
    {
        switch (field.length())
        {
            case 6:
                if (field == "expect")
                {
                    expect_ = value;
                }
                break;
            case 10:
            {
                if (field == "connection")
                {
                    if (version_ == Version::kHttp11)
                    {
                        if (value.length() == 5 && value == "close")
                            keepAlive_ = false;
                    }
                    else if (value.length() == 10 &&
                             (value == "Keep-Alive" || value == "keep-alive"))
                    {
                        keepAlive_ = true;
                    }
                }
            }
            break;
            case 14:
                if (field == "content-length")
                {
                    contentLen_ = std::stoull(value.c_str());
                }
                break;
            default:
                break;
        }
        headers_.emplace(std::move(field), std::move(value));
    }
}

HttpRequestPtr HttpRequest::newHttpRequest()
{
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Get);
    req->setVersion(drogon::Version::kHttp11);
    return req;
}

HttpRequestPtr HttpRequest::newHttpFormPostRequest()
{
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Post);
    req->setVersion(drogon::Version::kHttp11);
    req->contentType_ = CT_APPLICATION_X_FORM;
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
    req->setVersion(drogon::Version::kHttp11);
    req->contentType_ = CT_APPLICATION_JSON;
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
    using std::swap;
    swap(method_, that.method_);
    swap(version_, that.version_);
    swap(flagForParsingJson_, that.flagForParsingJson_);
    swap(flagForParsingParameters_, that.flagForParsingParameters_);
    swap(matchedPathPattern_, that.matchedPathPattern_);
    swap(path_, that.path_);
    swap(query_, that.query_);
    swap(headers_, that.headers_);
    swap(cookies_, that.cookies_);
    swap(parameters_, that.parameters_);
    swap(jsonPtr_, that.jsonPtr_);
    swap(sessionPtr_, that.sessionPtr_);
    swap(attributesPtr_, that.attributesPtr_);
    swap(cacheFilePtr_, that.cacheFilePtr_);
    swap(peer_, that.peer_);
    swap(local_, that.local_);
    swap(creationDate_, that.creationDate_);
    swap(content_, that.content_);
    swap(contentLen_, that.contentLen_);
    swap(expect_, that.expect_);
    swap(contentType_, that.contentType_);
    swap(contentTypeString_, that.contentTypeString_);
    swap(keepAlive_, that.keepAlive_);
    swap(loop_, that.loop_);
}

const char *HttpRequestImpl::methodString() const
{
    const char *result = "UNKNOWN";
    switch (method_)
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
    assert(method_ == Invalid);
    string_view m(start, end - start);
    switch (m.length())
    {
        case 3:
            if (m == "GET")
            {
                method_ = Get;
            }
            else if (m == "PUT")
            {
                method_ = Put;
            }
            else
            {
                method_ = Invalid;
            }
            break;
        case 4:
            if (m == "POST")
            {
                method_ = Post;
            }
            else if (m == "HEAD")
            {
                method_ = Head;
            }
            else
            {
                method_ = Invalid;
            }
            break;
        case 6:
            if (m == "DELETE")
            {
                method_ = Delete;
            }
            else
            {
                method_ = Invalid;
            }
            break;
        case 7:
            if (m == "OPTIONS")
            {
                method_ = Options;
            }
            else
            {
                method_ = Invalid;
            }
            break;
        default:
            method_ = Invalid;
            break;
    }

    // if (method_ != Invalid)
    // {
    //     content_ = "";
    //     query_ = "";
    //     cookies_.clear();
    //     parameters_.clear();
    //     headers_.clear();
    // }
    return method_ != Invalid;
}

HttpRequestImpl::~HttpRequestImpl()
{
}

void HttpRequestImpl::reserveBodySize()
{
    if (contentLen_ <=
        HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize())
    {
        content_.reserve(contentLen_);
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
        cacheFilePtr_ = std::make_unique<CacheFile>(tmpfile);
    }
}