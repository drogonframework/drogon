/**
 *
 *  @file HttpRequestImpl.cc
 *  @author An Tao
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

#include <zlib.h>
#ifdef USE_BROTLI
#include <brotli/decode.h>
#endif

using namespace drogon;
void HttpRequestImpl::parseJson() const
{
    auto input = contentView();
    if (input.empty())
        return;
    if (contentType_ == CT_APPLICATION_JSON ||
        getHeaderBy("content-type").find("application/json") !=
            std::string::npos)
    {
        static std::once_flag once;
        static Json::CharReaderBuilder builder;
        std::call_once(once, []() {
            builder["collectComments"] = false;
            builder["stackLimit"] = static_cast<Json::UInt>(
                drogon::app().getJsonParserStackLimit());
        });
        jsonPtr_ = std::make_shared<Json::Value>();
        JSONCPP_STRING errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(input.data(),
                           input.data() + input.size(),
                           jsonPtr_.get(),
                           &errs))
        {
            LOG_DEBUG << errs;
            jsonPtr_.reset();
            jsonParsingErrorPtr_ =
                std::make_unique<std::string>(std::move(errs));
        }
        else
        {
            jsonParsingErrorPtr_.reset();
        }
    }
    else
    {
        jsonPtr_.reset();
        jsonParsingErrorPtr_ =
            std::make_unique<std::string>("content type error");
    }
}
void HttpRequestImpl::parseParameters() const
{
    auto input = queryView();
    if (!input.empty())
    {
        string_view::size_type pos = 0;
        while ((input[pos] == '?' ||
                isspace(static_cast<unsigned char>(input[pos]))) &&
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
                while (cpos < key.length() &&
                       isspace(static_cast<unsigned char>(key[cpos])))
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
                while (cpos < key.length() &&
                       isspace(static_cast<unsigned char>(key[cpos])))
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
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return tolower(c);
    });
    if (type.empty() ||
        type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        string_view::size_type pos = 0;
        while ((input[pos] == '?' ||
                isspace(static_cast<unsigned char>(input[pos]))) &&
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
                while (cpos < key.length() &&
                       isspace(static_cast<unsigned char>(key[cpos])))
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
                while (cpos < key.length() &&
                       isspace(static_cast<unsigned char>(key[cpos])))
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
        case Patch:
            output->append("PATCH ");
            break;
        default:
            return;
    }

    if (!path_.empty())
    {
        if (pathEncode_)
        {
            output->append(utils::urlEncode(path_));
        }
        else
        {
            output->append(path_);
        }
    }
    else
    {
        output->append("/");
    }

    std::string content;
    if (passThrough_ && !query_.empty())
    {
        output->append("?");
        output->append(query_);
    }
    if (!passThrough_ && !parameters_.empty() &&
        contentType_ != CT_MULTIPART_FORM_DATA)
    {
        for (auto const &p : parameters_)
        {
            content.append(utils::urlEncodeComponent(p.first));
            content.append("=");
            content.append(utils::urlEncodeComponent(p.second));
            content.append("&");
        }
        content.resize(content.length() - 1);
        if (contentType_ != CT_APPLICATION_X_FORM)
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
                content.append("content-disposition: form-data; name=\"");
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
                content.append("content-disposition: form-data; name=\"");
                content.append(file.itemName());
                content.append("\"; filename=\"");
                content.append(file.fileName());
                content.append("\"");
                if (file.contentType() != CT_NONE)
                {
                    content.append("\r\n");

                    auto &type = contentTypeToMime(file.contentType());
                    content.append("content-type: ");
                    content.append(type.data(), type.length());
                }
                content.append("\r\n\r\n");
                std::ifstream infile(utils::toNativePath(file.path()),
                                     std::ifstream::binary);
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
    if (!passThrough_)
    {
        if (!content.empty() || !content_.empty())
        {
            char buf[64];
            auto len = snprintf(
                buf,
                sizeof(buf),
                contentLengthFormatString<decltype(content.length())>(),
                content.length() + content_.length());
            output->append(buf, len);
            if (contentTypeString_.empty())
            {
                auto &type = contentTypeToMime(contentType_);
                output->append("content-type: ");
                output->append(type.data(), type.length());
                output->append("\r\n");
            }
        }
        else if (method_ == Post || method_ == Put || method_ == Options ||
                 method_ == Patch)
        {
            output->append("content-length: 0\r\n", 19);
        }
        if (!contentTypeString_.empty())
        {
            output->append("content-type: ");
            output->append(contentTypeString_);
            output->append("\r\n");
        }
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
        output->append("cookie: ");
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it)
        {
            output->append(it->first);
            output->append("=");
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
    std::transform(field.begin(),
                   field.end(),
                   field.begin(),
                   [](unsigned char c) { return tolower(c); });
    ++colon;
    while (colon < end && isspace(static_cast<unsigned char>(*colon)))
    {
        ++colon;
    }
    std::string value(colon, end);
    while (!value.empty() &&
           isspace(static_cast<unsigned char>(value[value.size() - 1])))
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
                       isspace(static_cast<unsigned char>(cookie_name[cpos])))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                cpos = 0;
                while (cpos < cookie_value.length() &&
                       isspace(static_cast<unsigned char>(cookie_value[cpos])))
                    ++cpos;
                cookie_value = cookie_value.substr(cpos);
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
                       isspace(static_cast<unsigned char>(cookie_name[cpos])))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                std::string cookie_value = coo.substr(epos + 1);
                cpos = 0;
                while (cpos < cookie_value.length() &&
                       isspace(static_cast<unsigned char>(cookie_value[cpos])))
                    ++cpos;
                cookie_value = cookie_value.substr(cpos);
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
                    expectPtr_ = std::make_unique<std::string>(value);
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
    req->flagForParsingContentType_ = true;
    return req;
}

HttpRequestPtr HttpRequest::newHttpJsonRequest(const Json::Value &data)
{
    static std::once_flag once;
    static Json::StreamWriterBuilder builder;
    std::call_once(once, []() {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        if (!app().isUnicodeEscapingUsedInJson())
        {
            builder["emitUTF8"] = true;
        }
        auto &precision = app().getFloatPrecisionInJson();
        if (precision.first != 0)
        {
            builder["precision"] = precision.first;
            builder["precisionType"] = precision.second;
        }
    });
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(drogon::Get);
    req->setVersion(drogon::Version::kHttp11);
    req->contentType_ = CT_APPLICATION_JSON;
    req->setContent(writeString(builder, data));
    req->flagForParsingContentType_ = true;
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
    swap(originalPath_, that.originalPath_);
    swap(pathEncode_, that.pathEncode_);
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
    swap(expectPtr_, that.expectPtr_);
    swap(contentType_, that.contentType_);
    swap(contentTypeString_, that.contentTypeString_);
    swap(keepAlive_, that.keepAlive_);
    swap(loop_, that.loop_);
    swap(flagForParsingContentType_, that.flagForParsingContentType_);
    swap(jsonParsingErrorPtr_, that.jsonParsingErrorPtr_);
}

const char *HttpRequestImpl::versionString() const
{
    const char *result = "UNKNOWN";
    switch (version_)
    {
        case Version::kHttp10:
            result = "HTTP/1.0";
            break;

        case Version::kHttp11:
            result = "HTTP/1.1";
            break;

        default:
            break;
    }
    return result;
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
        case Patch:
            result = "PATCH";
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
        case 5:
            if (m == "PATCH")
            {
                method_ = Patch;
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

void HttpRequestImpl::reserveBodySize(size_t length)
{
    if (length <= HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize())
    {
        content_.reserve(length);
    }
    else
    {
        // Store data of body to a temperary file
        createTmpFile();
    }
}

void HttpRequestImpl::appendToBody(const char *data, size_t length)
{
    if (cacheFilePtr_)
    {
        cacheFilePtr_->append(data, length);
    }
    else
    {
        if (content_.length() + length <=
            HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize())
        {
            content_.append(data, length);
        }
        else
        {
            createTmpFile();
            cacheFilePtr_->append(content_);
            cacheFilePtr_->append(data, length);
            content_.clear();
        }
    }
}

void HttpRequestImpl::createTmpFile()
{
    auto tmpfile = HttpAppFrameworkImpl::instance().getUploadPath();
    auto fileName = utils::getUuid();
    tmpfile.append("/tmp/")
        .append(1, fileName[0])
        .append(1, fileName[1])
        .append("/")
        .append(fileName);
    cacheFilePtr_ = std::make_unique<CacheFile>(tmpfile);
}

void HttpRequestImpl::setContentTypeString(const char *typeString,
                                           size_t typeStringLength)
{
    std::string sv(typeString, typeStringLength);
    auto contentType = parseContentType(sv);
    if (contentType == CT_NONE)
        contentType = CT_CUSTOM;
    contentType_ = contentType;
    contentTypeString_ = std::string(sv);
    flagForParsingContentType_ = true;
}

StreamDecompressStatus HttpRequestImpl::decompressBody()
{
    auto &contentEncoding = getHeaderBy("content-encoding");
    if (contentEncoding.empty() || contentEncoding == "identity")
    {
        removeHeaderBy("content-encoding");
        return StreamDecompressStatus::Ok;
    }
#ifdef USE_BROTLI
    else if (contentEncoding == "br")
    {
        removeHeaderBy("content-encoding");
        return decompressBodyBrotli();
    }
#endif
    else if (contentEncoding == "gzip")
    {
        removeHeaderBy("content-encoding");
        return decompressBodyGzip();
    }
    return StreamDecompressStatus::NotSupported;
}

#ifdef USE_BROTLI
StreamDecompressStatus HttpRequestImpl::decompressBodyBrotli() noexcept
{
    // Workaround for Windows min and max are macros
    auto minVal = [](size_t a, size_t b) { return a < b ? a : b; };
    std::unique_ptr<CacheFile> cacheFileHolder;
    std::string contentHolder;
    string_view compressed;
    if (cacheFilePtr_)
    {
        cacheFileHolder = std::move(cacheFilePtr_);
        compressed = cacheFileHolder->getStringView();
    }
    else
    {
        contentHolder = std::move(content_);
        compressed = contentHolder;
    }

    setBody("");
    const size_t maxBodySize =
        HttpAppFrameworkImpl::instance().getClientMaxBodySize();
    const size_t maxMemorySize =
        HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize();

    size_t availableIn = compressed.size();
    auto nextIn = (const uint8_t *)(compressed.data());
    auto decompressed = std::string(minVal(maxMemorySize, availableIn * 3), 0);
    auto nextOut = (uint8_t *)(decompressed.data());
    size_t totalOut{0};
    auto s = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    size_t lastOut = 0;
    StreamDecompressStatus status = StreamDecompressStatus::Ok;
    while (true)
    {
        uint8_t *outPtr = (uint8_t *)decompressed.data();
        size_t availableOut = decompressed.size();
        auto result = BrotliDecoderDecompressStream(
            s, &availableIn, &nextIn, &availableOut, &outPtr, &totalOut);
        size_t outSize = totalOut - lastOut;
        lastOut = totalOut;

        if (totalOut > maxBodySize)
        {
            setBody("");
            status = StreamDecompressStatus::TooLarge;
            break;
        }

        if (result == BROTLI_DECODER_RESULT_SUCCESS)
        {
            appendToBody(decompressed.data(), outSize);
            break;
        }
        else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
        {
            appendToBody(decompressed.data(), outSize);
            size_t currentSize = decompressed.size();
            decompressed.clear();
            decompressed.resize(minVal(currentSize * 2, maxMemorySize));
        }
        else
        {
            setBody("");
            status = StreamDecompressStatus::DecompressError;
            break;
        }
    }
    BrotliDecoderDestroyInstance(s);
    return StreamDecompressStatus::Ok;
}
#endif

StreamDecompressStatus HttpRequestImpl::decompressBodyGzip() noexcept
{
    // Workaround for Windows min and max are macros
    auto minVal = [](size_t a, size_t b) { return a < b ? a : b; };
    std::unique_ptr<CacheFile> cacheFileHolder;
    std::string contentHolder;
    string_view compressed;
    if (cacheFilePtr_)
    {
        cacheFileHolder = std::move(cacheFilePtr_);
        compressed = cacheFileHolder->getStringView();
    }
    else
    {
        contentHolder = std::move(content_);
        compressed = contentHolder;
    }

    z_stream strm = {nullptr,
                     0,
                     0,
                     nullptr,
                     0,
                     0,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     0,
                     0,
                     0};
    strm.next_in = (Bytef *)compressed.data();
    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    setBody("");
    const size_t maxBodySize =
        HttpAppFrameworkImpl::instance().getClientMaxBodySize();
    const size_t maxMemorySize =
        HttpAppFrameworkImpl::instance().getClientMaxMemoryBodySize();
    auto decompressed =
        std::string(minVal(compressed.size() * 2, maxMemorySize), 0);
    strm.next_out = (Bytef *)decompressed.data();
    strm.avail_out = static_cast<uInt>(decompressed.size());
    size_t lastOut = 0;
    if (inflateInit2(&strm, (15 + 32)) != Z_OK)
    {
        return StreamDecompressStatus::DecompressError;
    }

    StreamDecompressStatus status = StreamDecompressStatus::Ok;
    while (true)
    {
        // Inflate another chunk.
        int decompressStatus = inflate(&strm, Z_SYNC_FLUSH);

        if (strm.total_out > maxBodySize)
        {
            setBody("");
            status = StreamDecompressStatus::TooLarge;
            break;
        }

        size_t outSize = strm.total_out - lastOut;
        lastOut = strm.total_out;
        if (decompressStatus == Z_STREAM_END)
        {
            appendToBody(decompressed.data(), outSize);
            break;
        }
        else if (decompressStatus != Z_OK)
        {
            setBody("");
            status = StreamDecompressStatus::DecompressError;
            break;
        }
        else
        {
            appendToBody(decompressed.data(), outSize);
            size_t currentSize = decompressed.size();
            decompressed.clear();
            decompressed.resize(minVal(currentSize * 2, maxMemorySize));
            strm.next_out = (Bytef *)decompressed.data();
            strm.avail_out = static_cast<uInt>(decompressed.size());
        }
    }
    if (inflateEnd(&strm) != Z_OK)
    {
        setBody("");
        if (status == StreamDecompressStatus::Ok)
            status = StreamDecompressStatus::DecompressError;
        return status;
    }
    return status;
}
