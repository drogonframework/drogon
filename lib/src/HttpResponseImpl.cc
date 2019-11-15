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

#include "HttpResponseImpl.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpUtils.h"
#include <drogon/HttpViewData.h>
#include <drogon/IOThreadStorage.h>
#include <fstream>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <trantor/utils/Logger.h>

using namespace trantor;
using namespace drogon;

namespace drogon
{
// "Fri, 23 Aug 2019 12:58:03 GMT" length = 29
static const size_t httpFullDateStringLength = 29;
static HttpResponsePtr genHttpResponse(std::string viewName,
                                       const HttpViewData &data)
{
    auto templ = DrTemplateBase::newTemplate(viewName);
    if (templ)
    {
        auto res = HttpResponse::newHttpResponse();
        res->setStatusCode(k200OK);
        res->setContentTypeCode(CT_TEXT_HTML);
        res->setBody(templ->genText(data));
        return res;
    }
    return drogon::HttpResponse::newNotFoundResponse();
}
}  // namespace drogon

HttpResponsePtr HttpResponse::newHttpResponse()
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_TEXT_HTML);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(const Json::Value &data)
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_APPLICATION_JSON);
    res->setJsonObject(data);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(Json::Value &&data)
{
    auto res = std::make_shared<HttpResponseImpl>(k200OK, CT_APPLICATION_JSON);
    res->setJsonObject(std::move(data));
    return res;
}

void HttpResponseImpl::generateBodyFromJson()
{
    if (!_jsonPtr)
    {
        return;
    }
    static std::once_flag once;
    static Json::StreamWriterBuilder builder;
    std::call_once(once, []() {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
    });
    setBody(writeString(builder, *_jsonPtr));
}

HttpResponsePtr HttpResponse::newNotFoundResponse()
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
        if (loop && loop->index() < app().getThreadNum())
        {
            // If the current thread is an IO thread
            static std::once_flag threadOnce;
            static IOThreadStorage<HttpResponsePtr> thread404Pages;
            std::call_once(threadOnce, [] {
                thread404Pages.init([](drogon::HttpResponsePtr &resp,
                                       size_t index) {
                    HttpViewData data;
                    data.insert("version", getVersion());
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
            data.insert("version", getVersion());
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
    return res;
}

HttpResponsePtr HttpResponse::newHttpViewResponse(const std::string &viewName,
                                                  const HttpViewData &data)
{
    return genHttpResponse(viewName, data);
}

HttpResponsePtr HttpResponse::newFileResponse(
    const std::string &fullPath,
    const std::string &attachmentFileName,
    ContentType type)
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
    pbuf->pubseekoff(0, infile.beg);  // rewind
    if (HttpAppFrameworkImpl::instance().useSendfile() && filesize > 1024 * 200)
    // TODO : Is 200k an appropriate value? Or set it to be configurable
    {
        // The advantages of sendfile() can only be reflected in sending large
        // files.
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
        resp->setContentTypeCode(type);
    }

    if (!attachmentFileName.empty())
    {
        resp->addHeader("Content-Disposition",
                        "attachment; filename=" + attachmentFileName);
    }

    return resp;
}

void HttpResponseImpl::makeHeaderString(
    const std::shared_ptr<std::string> &headerStringPtr)
{
    char buf[128];
    assert(headerStringPtr);
    auto len = snprintf(buf, sizeof buf, "HTTP/1.1 %d ", _statusCode);
    headerStringPtr->append(buf, len);
    if (!_statusMessage.empty())
        headerStringPtr->append(_statusMessage.data(), _statusMessage.length());
    headerStringPtr->append("\r\n");
    generateBodyFromJson();
    if (_sendfileName.empty())
    {
        long unsigned int bodyLength =
            _bodyPtr ? _bodyPtr->length()
                     : (_bodyViewPtr ? _bodyViewPtr->length() : 0);
        len = snprintf(buf, sizeof buf, "Content-Length: %lu\r\n", bodyLength);
    }
    else
    {
        struct stat filestat;
        if (stat(_sendfileName.c_str(), &filestat) < 0)
        {
            LOG_SYSERR << _sendfileName << " stat error";
            return;
        }
        len = snprintf(buf,
                       sizeof buf,
                       "Content-Length: %llu\r\n",
                       static_cast<long long unsigned int>(filestat.st_size));
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
            // output->append("Connection: Keep-Alive\r\n");
        }
    }
    headerStringPtr->append(_contentTypeString.data(),
                            _contentTypeString.length());
    for (auto it = _headers.begin(); it != _headers.end(); ++it)
    {
        headerStringPtr->append(it->first);
        headerStringPtr->append(": ");
        headerStringPtr->append(it->second);
        headerStringPtr->append("\r\n");
    }
    if (HttpAppFrameworkImpl::instance().sendServerHeader())
    {
        headerStringPtr->append(
            HttpAppFrameworkImpl::instance().getServerHeaderString());
    }
}
void HttpResponseImpl::renderToBuffer(trantor::MsgBuffer &buffer)
{
    if (_expriedTime >= 0)
    {
        auto strPtr = renderToString();
        buffer.append(strPtr->data(), strPtr->length());
        return;
    }

    if (!_fullHeaderString)
    {
        char buf[128];
        auto len = snprintf(buf, sizeof buf, "HTTP/1.1 %d ", _statusCode);
        buffer.append(buf, len);
        if (!_statusMessage.empty())
            buffer.append(_statusMessage.data(), _statusMessage.length());
        buffer.append("\r\n");
        generateBodyFromJson();
        if (_sendfileName.empty())
        {
            long unsigned int bodyLength =
                _bodyPtr ? _bodyPtr->length()
                         : (_bodyViewPtr ? _bodyViewPtr->length() : 0);
            len = snprintf(buf,
                           sizeof buf,
                           "Content-Length: %lu\r\n",
                           bodyLength);
        }
        else
        {
            struct stat filestat;
            if (stat(_sendfileName.c_str(), &filestat) < 0)
            {
                LOG_SYSERR << _sendfileName << " stat error";
                return;
            }
            len =
                snprintf(buf,
                         sizeof buf,
                         "Content-Length: %llu\r\n",
                         static_cast<long long unsigned int>(filestat.st_size));
        }

        buffer.append(buf, len);
        if (_headers.find("Connection") == _headers.end())
        {
            if (_closeConnection)
            {
                buffer.append("Connection: close\r\n");
            }
            else
            {
                // output->append("Connection: Keep-Alive\r\n");
            }
        }
        buffer.append(_contentTypeString.data(), _contentTypeString.length());
        for (auto it = _headers.begin(); it != _headers.end(); ++it)
        {
            buffer.append(it->first);
            buffer.append(": ");
            buffer.append(it->second);
            buffer.append("\r\n");
        }
        if (HttpAppFrameworkImpl::instance().sendServerHeader())
        {
            buffer.append(
                HttpAppFrameworkImpl::instance().getServerHeaderString());
        }
    }
    else
    {
        buffer.append(*_fullHeaderString);
    }

    // output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {
            buffer.append(it->second.cookieString());
        }
    }

    // output Date header
    if (drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        buffer.append("Date: ");
        buffer.append(utils::getHttpFullDate(trantor::Date::date()),
                      httpFullDateStringLength);
        buffer.append("\r\n\r\n");
    }
    else
    {
        buffer.append("\r\n");
    }
    if (_bodyPtr)
        buffer.append(*_bodyPtr);
    else if (_bodyViewPtr)
        buffer.append(_bodyViewPtr->data(), _bodyViewPtr->length());
}
std::shared_ptr<std::string> HttpResponseImpl::renderToString()
{
    if (_expriedTime >= 0)
    {
        if (drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
        {
            if (_datePos != std::string::npos)
            {
                auto now = trantor::Date::now();
                bool isDateChanged =
                    ((now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC) !=
                     _httpStringDate);
                assert(_httpString);
                if (isDateChanged)
                {
                    _httpStringDate =
                        now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
                    auto newDate = utils::getHttpFullDate(now);

                    _httpString = std::make_shared<std::string>(*_httpString);
                    memcpy((void *)&(*_httpString)[_datePos],
                           newDate,
                           httpFullDateStringLength);
                    return _httpString;
                }

                return _httpString;
            }
        }
        else
        {
            if (_httpString)
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

    // output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {
            httpString->append(it->second.cookieString());
        }
    }

    // output Date header
    if (drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        httpString->append("Date: ");
        auto datePos = httpString->length();
        httpString->append(utils::getHttpFullDate(trantor::Date::date()),
                           httpFullDateStringLength);
        httpString->append("\r\n\r\n");
        _datePos = datePos;
    }
    else
    {
        httpString->append("\r\n");
    }

    LOG_TRACE << "reponse(no body):" << httpString->c_str();
    if (_bodyPtr)
        httpString->append(*_bodyPtr);
    else if (_bodyViewPtr)
        httpString->append(_bodyViewPtr->data(), _bodyViewPtr->length());
    if (_expriedTime >= 0)
    {
        _httpString = httpString;
    }
    return httpString;
}

std::shared_ptr<std::string> HttpResponseImpl::renderHeaderForHeadMethod()
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

    // output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {
            httpString->append(it->second.cookieString());
        }
    }

    // output Date header
    if (drogon::HttpAppFrameworkImpl::instance().sendDateHeader())
    {
        httpString->append("Date: ");
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
    _fullHeaderString.reset();
    std::string field(start, colon);
    transform(field.begin(), field.end(), field.begin(), ::tolower);
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

    if (field == "set-cookie")
    {
        // LOG_INFO<<"cookies!!!:"<<value;
        auto values = utils::splitString(value, ";");
        Cookie cookie;
        cookie.setHttpOnly(false);
        for (size_t i = 0; i < values.size(); i++)
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
                       isspace(cookie_name[cpos]))
                    ++cpos;
                cookie_name = cookie_name.substr(cpos);
                ++epos;
                while (epos < coo.length() && isspace(coo[epos]))
                    ++epos;
                cookie_value = coo.substr(epos);
            }
            else
            {
                std::string::size_type cpos = 0;
                while (cpos < coo.length() && isspace(coo[cpos]))
                    cpos++;
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
                               tolower);
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
            }
        }
        if (!cookie.key().empty())
        {
            _cookies[cookie.key()] = cookie;
        }
    }
    else
    {
        _headers[std::move(field)] = std::move(value);
    }
}

void HttpResponseImpl::swap(HttpResponseImpl &that) noexcept
{
    using std::swap;
    _headers.swap(that._headers);
    _cookies.swap(that._cookies);
    swap(_statusCode, that._statusCode);
    swap(_v, that._v);
    swap(_statusMessage, that._statusMessage);
    swap(_closeConnection, that._closeConnection);
    _bodyPtr.swap(that._bodyPtr);
    _bodyViewPtr.swap(that._bodyViewPtr);
    swap(_leftBodyLength, that._leftBodyLength);
    swap(_currentChunkLength, that._currentChunkLength);
    swap(_contentType, that._contentType);
    _jsonPtr.swap(that._jsonPtr);
    _fullHeaderString.swap(that._fullHeaderString);
    _httpString.swap(that._httpString);
    swap(_datePos, that._datePos);
}

void HttpResponseImpl::clear()
{
    _statusCode = kUnknown;
    _v = kHttp11;
    _statusMessage = string_view{};
    _fullHeaderString.reset();
    _headers.clear();
    _cookies.clear();
    _bodyPtr.reset();
    _bodyViewPtr.reset();
    _leftBodyLength = 0;
    _currentChunkLength = 0;
    _jsonPtr.reset();
    _expriedTime = -1;
    _datePos = std::string::npos;
}

void HttpResponseImpl::parseJson() const
{
    static std::once_flag once;
    static Json::CharReaderBuilder builder;
    std::call_once(once, []() { builder["collectComments"] = false; });
    _jsonPtr = std::make_shared<Json::Value>();
    JSONCPP_STRING errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (_bodyPtr)
    {
        if (!reader->parse(_bodyPtr->data(),
                           _bodyPtr->data() + _bodyPtr->size(),
                           _jsonPtr.get(),
                           &errs))
        {
            LOG_ERROR << errs;
            LOG_ERROR << "body: " << *_bodyPtr;
            _jsonPtr.reset();
        }
    }
    else if (_bodyViewPtr)
    {
        if (!reader->parse(_bodyViewPtr->data(),
                           _bodyViewPtr->data() + _bodyViewPtr->size(),
                           _jsonPtr.get(),
                           &errs))
        {
            LOG_ERROR << errs;
            _jsonPtr.reset();
        }
    }
}

HttpResponseImpl::~HttpResponseImpl()
{
}
