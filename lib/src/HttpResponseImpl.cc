/**
 *
 *  @file HttpResponseImpl.cc
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

#include "HttpResponseImpl.h"
#include "AOPAdvice.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpUtils.h"
#include <drogon/HttpViewData.h>
#include <drogon/IOThreadStorage.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <trantor/utils/Logger.h>

using namespace trantor;
using namespace drogon;
using namespace std::literals::string_literals;
using namespace std::placeholders;
#ifdef _WIN32
#undef max
#endif

namespace drogon
{
// "Fri, 23 Aug 2019 12:58:03 GMT" length = 29
static const size_t httpFullDateStringLength = 29;

static inline HttpResponsePtr genHttpResponse(const std::string &viewName,
                                              const HttpViewData &data,
                                              const HttpRequestPtr &req)
{
    auto templ = DrTemplateBase::newTemplate(viewName);
    if (templ)
    {
        auto res = HttpResponse::newHttpResponse();
        res->setBody(templ->genText(data));
        return res;
    }
    return drogon::HttpResponse::newNotFoundResponse(req);
}
}  // namespace drogon

HttpResponsePtr HttpResponse::newHttpResponse()
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_TEXT_HTML);
    AopAdvice::instance().passResponseCreationAdvices(res);
    return res;
}

HttpResponsePtr HttpResponse::newHttpResponse(HttpStatusCode code,
                                              ContentType type)
{
    auto res = std::make_shared<HttpResponseImpl>(code, type);
    AopAdvice::instance().passResponseCreationAdvices(res);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(const Json::Value &data)
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_APPLICATION_JSON);
    res->setJsonObject(data);
    AopAdvice::instance().passResponseCreationAdvices(res);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(Json::Value &&data)
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_APPLICATION_JSON);
    res->setJsonObject(std::move(data));
    AopAdvice::instance().passResponseCreationAdvices(res);
    return res;
}

const char *HttpResponseImpl::versionString() const
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

void HttpResponseImpl::generateBodyFromJson() const
{
    if (!jsonPtr_ || flagForSerializingJson_)
    {
        return;
    }
    flagForSerializingJson_ = true;
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
    bodyPtr_ = std::make_shared<HttpMessageStringBody>(
        writeString(builder, *jsonPtr_));
}

HttpResponsePtr HttpResponse::newNotFoundResponse(const HttpRequestPtr &req)
{
    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
    auto &resp = HttpAppFrameworkImpl::instance().getCustom404Page();
    if (resp)
    {
        if (loop && loop->index() < app().getThreadNum())
        {
            return resp;
        }
        else
        {
            return HttpResponsePtr{new HttpResponseImpl(
                *static_cast<HttpResponseImpl *>(resp.get()))};
        }
    }
    else
    {
        if (HttpAppFrameworkImpl::instance().isUsingCustomErrorHandler())
        {
            return app().getCustomErrorHandler()(k404NotFound, req);
        }
        else if (loop && loop->index() < app().getThreadNum())
        {
            // If the current thread is an IO thread
            static std::once_flag threadOnce;
            static IOThreadStorage<HttpResponsePtr> thread404Pages;
            std::call_once(threadOnce, [req = req] {
                thread404Pages.init([req = req](drogon::HttpResponsePtr &resp,
                                                size_t /*index*/) {
                    HttpViewData data;
                    data.insert("version", drogon::getVersion());
                    resp = HttpResponse::newHttpViewResponse("drogon::NotFound",
                                                             data);
                    resp->setStatusCode(k404NotFound);
                    resp->setExpiredTime(0);
                });
            });
            LOG_TRACE << "Use cached 404 response";
            return thread404Pages.getThreadData();
        }
        else
        {
            HttpViewData data;
            data.insert("version", drogon::getVersion());
            auto notFoundResp =
                HttpResponse::newHttpViewResponse("drogon::NotFound", data);
            notFoundResp->setStatusCode(k404NotFound);
            return notFoundResp;
        }
    }
}

HttpResponsePtr HttpResponse::newRedirectionResponse(
    const std::string &location,
    HttpStatusCode status)
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(status);
    res->redirect(location);
    AopAdvice::instance().passResponseCreationAdvices(res);
    return res;
}

HttpResponsePtr HttpResponse::newHttpViewResponse(const std::string &viewName,
                                                  const HttpViewData &data,
                                                  const HttpRequestPtr &req)
{
    return genHttpResponse(viewName, data, req);
}

HttpResponsePtr HttpResponse::newFileResponse(
    const unsigned char *pBuffer,
    size_t bufferLength,
    const std::string &attachmentFileName,
    ContentType type,
    const std::string &typeString)
{
    // Make Raw HttpResponse
    auto resp = std::make_shared<HttpResponseImpl>();

    // Set response body and length
    resp->setBody(
        std::string(reinterpret_cast<const char *>(pBuffer), bufferLength));

    // Set status of message
    resp->setStatusCode(k200OK);

    // Check for type and assign proper content type in header
    if (!typeString.empty())
    {
        if (type == CT_NONE)
            type = parseContentType(typeString);
        if (type == CT_NONE)
            type = CT_APPLICATION_OCTET_STREAM;  // XXX: Is this Ok?
        static_cast<HttpResponse *>(resp.get())
            ->setContentTypeCodeAndCustomString(type,
                                                typeString.c_str(),
                                                typeString.size());
    }
    else if (type != CT_NONE)
    {
        resp->setContentTypeCode(type);
    }
    else if (!attachmentFileName.empty())
    {
        resp->setContentTypeCode(drogon::getContentType(attachmentFileName));
    }
    else
    {
        resp->setContentTypeCode(
            CT_APPLICATION_OCTET_STREAM);  // default content-type for file;
    }

    // Add additional header values
    if (!attachmentFileName.empty())
    {
        resp->addHeader("Content-Disposition",
                        "attachment; filename=" + attachmentFileName);
    }

    // Finalize and return response
    AopAdvice::instance().passResponseCreationAdvices(resp);
    return resp;
}

HttpResponsePtr HttpResponse::newFileResponse(
    const std::string &fullPath,
    const std::string &attachmentFileName,
    ContentType type,
    const std::string &typeString,
    const HttpRequestPtr &req)
{
    return newFileResponse(
        fullPath, 0, 0, false, attachmentFileName, type, typeString, req);
}

HttpResponsePtr HttpResponse::newFileResponse(
    const std::string &fullPath,
    size_t offset,
    size_t length,
    bool setContentRange,
    const std::string &attachmentFileName,
    ContentType type,
    const std::string &typeString,
    const HttpRequestPtr &req)
{
    std::ifstream infile(utils::toNativePath(fullPath), std::ifstream::binary);
    LOG_TRACE << "send http file:" << fullPath << " offset " << offset
              << " length " << length;
    if (!infile)
    {
        auto resp = HttpResponse::newNotFoundResponse(req);
        return resp;
    }
    auto resp = std::make_shared<HttpResponseImpl>();
    std::streambuf *pbuf = infile.rdbuf();
    size_t filesize =
        static_cast<size_t>(pbuf->pubseekoff(0, std::ifstream::end));
    if (offset > filesize || length > filesize ||  // in case of overflow
        offset + length > filesize)
    {
        resp->setStatusCode(k416RequestedRangeNotSatisfiable);
        if (setContentRange)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "bytes */%zu", filesize);
            resp->addHeader("Content-Range", std::string(buf));
        }
        return resp;
    }
    if (length == 0)
    {
        length = filesize - offset;
    }
    pbuf->pubseekoff(offset, std::ifstream::beg);  // rewind

    if (HttpAppFrameworkImpl::instance().useSendfile() && length > 1024 * 200)
    // TODO : Is 200k an appropriate value? Or set it to be configurable
    {
        // The advantages of sendfile() can only be reflected in sending large
        // files.
        resp->setSendfile(fullPath);
        // Must set length with the right value! Content-Length header relies on
        // this value.
        resp->setSendfileRange(offset, length);
    }
    else
    {
        std::string str;
        str.resize(length);
        pbuf->sgetn(&str[0], length);
        resp->setBody(std::move(str));
        resp->setSendfileRange(offset, length);
    }

    // Set correct status code
    if (length < filesize)
    {
        resp->setStatusCode(k206PartialContent);
    }
    else
    {
        resp->setStatusCode(k200OK);
    }

    // Infer content type
    if (type == CT_NONE)
    {
        if (!typeString.empty())
        {
            auto r = static_cast<HttpResponse *>(resp.get());
            if (type == CT_NONE)
                type = parseContentType(typeString);
            if (type == CT_NONE)
                type = CT_CUSTOM;  // XXX: Is this Ok?
            r->setContentTypeCodeAndCustomString(type, typeString);
        }
        else if (!attachmentFileName.empty())
        {
            resp->setContentTypeCode(
                drogon::getContentType(attachmentFileName));
        }
        else
        {
            resp->setContentTypeCode(drogon::getContentType(fullPath));
        }
    }
    else
    {
        if (typeString.empty())
            resp->setContentTypeCode(type);
        else
        {
            auto r = static_cast<HttpResponse *>(resp.get());
            if (type == CT_NONE)
                type = parseContentType(typeString);
            if (type == CT_NONE)
                type = CT_CUSTOM;  // XXX: Is this Ok?
            r->setContentTypeCodeAndCustomString(type, typeString);
        }
    }

    // Set headers
    if (!attachmentFileName.empty())
    {
        resp->addHeader("Content-Disposition",
                        "attachment; filename=" + attachmentFileName);
    }
    if (setContentRange && length > 0)
    {
        char buf[128];
        snprintf(buf,
                 sizeof(buf),
                 "bytes %zu-%zu/%zu",
                 offset,
                 offset + length - 1,
                 filesize);
        resp->addHeader("Content-Range", std::string(buf));
    }
    AopAdvice::instance().passResponseCreationAdvices(resp);
    return resp;
}

HttpResponsePtr HttpResponse::newStreamResponse(
    const std::function<std::size_t(char *, std::size_t)> &callback,
    const std::string &attachmentFileName,
    ContentType type,
    const std::string &typeString,
    const HttpRequestPtr &req)
{
    LOG_TRACE << "send stream as "s
              << (attachmentFileName.empty() ? "raw data"s
                                             : "file: "s + attachmentFileName);
    if (!callback)
    {
        auto resp = HttpResponse::newNotFoundResponse();
        return resp;
    }
    auto resp = std::make_shared<HttpResponseImpl>();
    resp->setStreamCallback(callback);
    resp->setStatusCode(k200OK);

    // Infer content type
    if (type == CT_NONE)
    {
        if (!typeString.empty())
        {
            auto r = static_cast<HttpResponse *>(resp.get());
            if (type == CT_NONE)
                type = parseContentType(typeString);
            if (type == CT_NONE)
                type = CT_CUSTOM;  // XXX: Is this Ok?
            r->setContentTypeCodeAndCustomString(type, typeString);
        }
        else if (!attachmentFileName.empty())
        {
            resp->setContentTypeCode(
                drogon::getContentType(attachmentFileName));
        }
    }
    else
    {
        if (typeString.empty())
            resp->setContentTypeCode(type);
        else
        {
            auto r = static_cast<HttpResponse *>(resp.get());
            if (type == CT_NONE)
                type = parseContentType(typeString);
            if (type == CT_NONE)
                type = CT_CUSTOM;  // XXX: Is this Ok?
            r->setContentTypeCodeAndCustomString(type, typeString);
        }
    }

    // Set headers
    if (!attachmentFileName.empty())
    {
        resp->addHeader("Content-Disposition",
                        "attachment; filename=" + attachmentFileName);
    }
    AopAdvice::instance().passResponseCreationAdvices(resp);
    return resp;
}

HttpResponsePtr HttpResponse::newAsyncStreamResponse(
    const std::function<void(ResponseStreamPtr)> &callback,
    bool disableKickoffTimeout)
{
    if (!callback)
    {
        auto resp = HttpResponse::newNotFoundResponse();
        return resp;
    }
    auto resp = std::make_shared<HttpResponseImpl>();
    resp->setAsyncStreamCallback(callback, disableKickoffTimeout);
    resp->setStatusCode(k200OK);
    AopAdvice::instance().passResponseCreationAdvices(resp);
    return resp;
}

void HttpResponseImpl::makeHeaderString(trantor::MsgBuffer &buffer)
{
    buffer.ensureWritableBytes(128);
    int len{0};
    if (version_ == Version::kHttp11)
    {
        if (customStatusCode_ >= 0)
        {
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           "HTTP/1.1 %d ",
                           customStatusCode_);
        }
        else
        {
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           "HTTP/1.1 %d ",
                           statusCode_);
        }
    }
    else
    {
        if (customStatusCode_ >= 0)
        {
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           "HTTP/1.0 %d ",
                           customStatusCode_);
        }
        else
        {
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           "HTTP/1.0 %d ",
                           statusCode_);
        }
    }

    buffer.hasWritten(len);

    if (!statusMessage_.empty())
        buffer.append(statusMessage_.data(), statusMessage_.length());
    buffer.append("\r\n");
    generateBodyFromJson();
    if (!passThrough_)
    {
        buffer.ensureWritableBytes(64);
        if (!contentLengthIsAllowed())
        {
            len = 0;
            if ((bodyPtr_ && bodyPtr_->length() > 0) ||
                !sendfileName_.empty() || streamCallback_ ||
                asyncStreamCallback_)
            {
                LOG_ERROR << "The body should be empty when the content-length "
                             "is not allowed!";
            }
        }
        else if (streamCallback_ || asyncStreamCallback_)
        {
            // When the headers are created, it is time to set the transfer
            // encoding to chunked if the contents size is not specified
            if (!ifCloseConnection() &&
                headers_.find("content-length") == headers_.end())
            {
                LOG_DEBUG << "send stream with transfer-encoding chunked";
                headers_["transfer-encoding"] = "chunked";
            }
            len = 0;
        }
        else if (sendfileName_.empty())
        {
            auto bodyLength = bodyPtr_ ? bodyPtr_->length() : 0;
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           contentLengthFormatString<decltype(bodyLength)>(),
                           bodyLength);
        }
        else
        {
            auto bodyLength = sendfileRange_.second;
            len = snprintf(buffer.beginWrite(),
                           buffer.writableBytes(),
                           contentLengthFormatString<decltype(bodyLength)>(),
                           bodyLength);
        }
        buffer.hasWritten(len);
        if (headers_.find("connection") == headers_.end())
        {
            if (closeConnection_)
            {
                buffer.append("connection: close\r\n");
            }
            else if (version_ == Version::kHttp10)
            {
                buffer.append("connection: Keep-Alive\r\n");
            }
        }

        if (!contentTypeString_.empty())
        {
            buffer.append("content-type: ");
            buffer.append(contentTypeString_);
            buffer.append("\r\n");
        }
        if (HttpAppFrameworkImpl::instance().sendServerHeader())
        {
            buffer.append(
                HttpAppFrameworkImpl::instance().getServerHeaderString());
        }
    }

    for (auto it = headers_.begin(); it != headers_.end(); ++it)
    {
        buffer.append(it->first);
        buffer.append(": ");
        buffer.append(it->second);
        buffer.append("\r\n");
    }
}

void HttpResponseImpl::renderToBuffer(trantor::MsgBuffer &buffer)
{
    if (expriedTime_ >= 0)
    {
        auto strPtr = renderToBuffer();
        buffer.append(strPtr->peek(), strPtr->readableBytes());
        return;
    }

    if (!fullHeaderString_)
    {
        makeHeaderString(buffer);
    }
    else
    {
        buffer.append(*fullHeaderString_);
    }

    // output cookies
    if (!cookies_.empty())
    {
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it)
        {
            buffer.append(it->second.cookieString());
        }
    }

    // output Date header
    if (!passThrough_ &&
        drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        buffer.append("date: ");
        buffer.append(utils::getHttpFullDateStr(trantor::Date::date()));
        buffer.append("\r\n\r\n");
    }
    else
    {
        buffer.append("\r\n");
    }
    if (bodyPtr_ && contentLengthIsAllowed())
        buffer.append(bodyPtr_->data(), bodyPtr_->length());
}

std::shared_ptr<trantor::MsgBuffer> HttpResponseImpl::renderToBuffer()
{
    if (expriedTime_ >= 0)
    {
        if (!passThrough_ &&
            drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
        {
            if (datePos_ != static_cast<size_t>(-1))
            {
                auto now = trantor::Date::now();
                bool isDateChanged =
                    ((now.microSecondsSinceEpoch() /
                      trantor::Date::MICRO_SECONDS_PER_SEC) != httpStringDate_);
                assert(httpString_);
                if (isDateChanged)
                {
                    httpStringDate_ = now.microSecondsSinceEpoch() /
                                      trantor::Date::MICRO_SECONDS_PER_SEC;
                    auto newDate = utils::getHttpFullDate(now);

                    httpString_ =
                        std::make_shared<trantor::MsgBuffer>(*httpString_);
                    memcpy((void *)&(*httpString_)[datePos_],
                           newDate,
                           httpFullDateStringLength);
                    return httpString_;
                }

                return httpString_;
            }
        }
        else
        {
            if (httpString_)
                return httpString_;
        }
    }
    auto httpString = std::make_shared<trantor::MsgBuffer>(256);
    if (!fullHeaderString_)
    {
        makeHeaderString(*httpString);
    }
    else
    {
        httpString->append(*fullHeaderString_);
    }

    // output cookies
    if (!cookies_.empty())
    {
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it)
        {
            httpString->append(it->second.cookieString());
        }
    }

    // output Date header
    if (!passThrough_ &&
        drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        httpString->append("date: ");
        auto datePos = httpString->readableBytes();
        httpString->append(utils::getHttpFullDateStr(trantor::Date::date()));
        httpString->append("\r\n\r\n");
        datePos_ = datePos;
    }
    else
    {
        httpString->append("\r\n");
    }

    LOG_TRACE << "response(no body):"
              << std::string_view{httpString->peek(),
                                  httpString->readableBytes()};
    if (bodyPtr_)
        httpString->append(bodyPtr_->data(), bodyPtr_->length());
    if (expriedTime_ >= 0)
    {
        httpString_ = httpString;
    }
    return httpString;
}

std::shared_ptr<trantor::MsgBuffer> HttpResponseImpl::
    renderHeaderForHeadMethod()
{
    auto httpString = std::make_shared<trantor::MsgBuffer>(256);
    if (!fullHeaderString_)
    {
        makeHeaderString(*httpString);
    }
    else
    {
        httpString->append(*fullHeaderString_);
    }

    // output cookies
    if (!cookies_.empty())
    {
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it)
        {
            httpString->append(it->second.cookieString());
        }
    }

    // output Date header
    if (!passThrough_ &&
        drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        httpString->append("date: ");
        httpString->append(utils::getHttpFullDate(trantor::Date::date()),
                           httpFullDateStringLength);
        httpString->append("\r\n\r\n");
    }
    else
    {
        httpString->append("\r\n");
    }

    return httpString;
}

void HttpResponseImpl::addHeader(const char *start,
                                 const char *colon,
                                 const char *end)
{
    fullHeaderString_.reset();
    std::string field(start, colon);
    transform(field.begin(), field.end(), field.begin(), [](unsigned char c) {
        return tolower(c);
    });
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

    if (field == "set-cookie")
    {
        // LOG_INFO<<"cookies!!!:"<<value;
        auto values = utils::splitString(value, ";");
        Cookie cookie;
        cookie.setHttpOnly(false);
        for (size_t i = 0; i < values.size(); ++i)
        {
            std::string &coo = values[i];
            std::string cookie_name;
            std::string cookie_value;
            auto epos = coo.find('=');
            if (epos != std::string::npos)
            {
                cookie_name = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < cookie_name.length() &&
                       isspace(static_cast<unsigned char>(cookie_name[cpos])))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                ++epos;
                while (epos < coo.length() &&
                       isspace(static_cast<unsigned char>(coo[epos])))
                    ++epos;
                cookie_value = coo.substr(epos);
            }
            else
            {
                std::string::size_type cpos = 0;
                while (cpos < coo.length() &&
                       isspace(static_cast<unsigned char>(coo[cpos])))
                    ++cpos;
                cookie_name = coo.substr(cpos);
            }
            if (i == 0)
            {
                cookie.setKey(cookie_name);
                cookie.setValue(cookie_value);
            }
            else
            {
                std::transform(cookie_name.begin(),
                               cookie_name.end(),
                               cookie_name.begin(),
                               [](unsigned char c) { return tolower(c); });
                if (cookie_name == "path")
                {
                    cookie.setPath(cookie_value);
                }
                else if (cookie_name == "domain")
                {
                    cookie.setDomain(cookie_value);
                }
                else if (cookie_name == "expires")
                {
                    cookie.setExpiresDate(utils::getHttpDate(cookie_value));
                }
                else if (cookie_name == "secure")
                {
                    cookie.setSecure(true);
                }
                else if (cookie_name == "httponly")
                {
                    cookie.setHttpOnly(true);
                }
                else if (cookie_name == "samesite")
                {
                    cookie.setSameSite(
                        cookie.convertString2SameSite(cookie_value));
                }
                else if (cookie_name == "max-age")
                {
                    cookie.setMaxAge(std::stoi(cookie_value));
                }
            }
        }
        if (!cookie.key().empty())
        {
            cookies_[cookie.key()] = std::move(cookie);
        }
    }
    else
    {
        headers_[std::move(field)] = std::move(value);
    }
}

void HttpResponseImpl::swap(HttpResponseImpl &that) noexcept
{
    using std::swap;
    headers_.swap(that.headers_);
    cookies_.swap(that.cookies_);
    swap(statusCode_, that.statusCode_);
    swap(version_, that.version_);
    swap(statusMessage_, that.statusMessage_);
    swap(closeConnection_, that.closeConnection_);
    bodyPtr_.swap(that.bodyPtr_);
    swap(contentType_, that.contentType_);
    swap(flagForParsingContentType_, that.flagForParsingContentType_);
    swap(flagForParsingJson_, that.flagForParsingJson_);
    swap(sendfileName_, that.sendfileName_);
    swap(streamCallback_, that.streamCallback_);
    swap(asyncStreamCallback_, that.asyncStreamCallback_);
    jsonPtr_.swap(that.jsonPtr_);
    fullHeaderString_.swap(that.fullHeaderString_);
    httpString_.swap(that.httpString_);
    swap(datePos_, that.datePos_);
    swap(jsonParsingErrorPtr_, that.jsonParsingErrorPtr_);
}

void HttpResponseImpl::clear()
{
    statusCode_ = kUnknown;
    version_ = Version::kHttp11;
    statusMessage_ = std::string_view{};
    fullHeaderString_.reset();
    jsonParsingErrorPtr_.reset();
    sendfileName_.clear();
    if (streamCallback_)
    {
        LOG_TRACE << "Cleanup HttpResponse stream callback";
        streamCallback_(nullptr, 0);  // callback internal cleanup
        streamCallback_ = {};
    }
    if (asyncStreamCallback_)
    {
        // asyncStreamCallback_(nullptr);
        asyncStreamCallback_ = {};
    }
    headers_.clear();
    cookies_.clear();
    bodyPtr_.reset();
    jsonPtr_.reset();
    expriedTime_ = -1;
    datePos_ = std::string::npos;
    flagForParsingContentType_ = false;
    flagForParsingJson_ = false;
}

void HttpResponseImpl::parseJson() const
{
    static std::once_flag once;
    static Json::CharReaderBuilder builder;
    std::call_once(once, []() {
        builder["collectComments"] = false;
        builder["stackLimit"] =
            static_cast<Json::UInt>(drogon::app().getJsonParserStackLimit());
    });
    JSONCPP_STRING errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (bodyPtr_)
    {
        jsonPtr_ = std::make_shared<Json::Value>();
        if (!reader->parse(bodyPtr_->data(),
                           bodyPtr_->data() + bodyPtr_->length(),
                           jsonPtr_.get(),
                           &errs))
        {
            LOG_ERROR << errs;
            LOG_ERROR << "body: " << bodyPtr_->getString();
            jsonPtr_.reset();
            jsonParsingErrorPtr_ =
                std::make_shared<std::string>(std::move(errs));
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
            std::make_shared<std::string>("empty response body");
    }
}

bool HttpResponseImpl::shouldBeCompressed() const
{
    if (streamCallback_ || asyncStreamCallback_ || !sendfileName_.empty() ||
        contentType() >= CT_APPLICATION_OCTET_STREAM ||
        getBody().length() < 1024 ||
        !(getHeaderBy("content-encoding").empty()) || !contentLengthIsAllowed())
    {
        return false;
    }
    return true;
}

void HttpResponseImpl::setContentTypeString(const char *typeString,
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
