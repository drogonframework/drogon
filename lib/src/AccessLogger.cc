/**
 *
 *  @file AccessLogger.cc
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

#include "HttpUtils.h"
#include <drogon/drogon.h>
#include <drogon/plugins/AccessLogger.h>
#include <regex>
#include <thread>
#if !defined _WIN32 && !defined __HAIKU__
#include <unistd.h>
#include <sys/syscall.h>
#elif defined __HAIKU__
#include <unistd.h>
#else
#include <sstream>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

using namespace drogon;
using namespace drogon::plugin;

void AccessLogger::initAndStart(const Json::Value &config)
{
    useLocalTime_ = config.get("use_local_time", true).asBool();
    showMicroseconds_ = config.get("show_microseconds", true).asBool();
    timeFormat_ = config.get("custom_time_format", "").asString();
    useCustomTimeFormat_ = !timeFormat_.empty();

    logFunctionMap_ = {{"$request_path", outputReqPath},
                       {"$path", outputReqPath},
                       {"$date",
                        [this](trantor::LogStream &stream,
                               const drogon::HttpRequestPtr &req,
                               const drogon::HttpResponsePtr &resp) {
                            outputDate(stream, req, resp);
                        }},
                       {"$request_date",
                        [this](trantor::LogStream &stream,
                               const drogon::HttpRequestPtr &req,
                               const drogon::HttpResponsePtr &resp) {
                            outputReqDate(stream, req, resp);
                        }},
                       {"$request_query", outputReqQuery},
                       {"$request_url", outputReqURL},
                       {"$query", outputReqQuery},
                       {"$url", outputReqURL},
                       {"$request_version", outputVersion},
                       {"$version", outputVersion},
                       {"$request", outputReqLine},
                       {"$remote_addr", outputRemoteAddr},
                       {"$local_addr", outputLocalAddr},
                       {"$request_len", outputReqLength},
                       {"$body_bytes_received", outputReqLength},
                       {"$method", outputMethod},
                       {"$thread", outputThreadNumber},
                       {"$response_len", outputRespLength},
                       {"$body_bytes_sent", outputRespLength},
                       {"$status", outputStatusString},
                       {"$status_code", outputStatusCode},
                       {"$processing_time", outputProcessingTime},
                       {"$upstream_http_content-type", outputRespContentType},
                       {"$upstream_http_content_type", outputRespContentType}};
    auto format = config.get("log_format", "").asString();
    if (format.empty())
    {
        format =
            "$request_date $method $url [$body_bytes_received] ($remote_addr - "
            "$local_addr) $status $body_bytes_sent $processing_time";
    }
    createLogFunctions(format);
    auto logPath = config.get("log_path", "").asString();
    if (!logPath.empty())
    {
        auto fileName = config.get("log_file", "access.log").asString();
        auto extension = std::string(".log");
        auto pos = fileName.rfind('.');
        if (pos != std::string::npos)
        {
            extension = fileName.substr(pos);
            fileName = fileName.substr(0, pos);
        }
        if (fileName.empty())
        {
            fileName = "access";
        }
        asyncFileLogger_.setFileName(fileName, extension, logPath);
        asyncFileLogger_.startLogging();
        logIndex_ = config.get("log_index", 0).asInt();
        trantor::Logger::setOutputFunction(
            [&](const char *msg, const uint64_t len) {
                asyncFileLogger_.output(msg, len);
            },
            [&]() { asyncFileLogger_.flush(); },
            logIndex_);
        auto sizeLimit = config.get("log_size_limit", 0).asUInt64();
        if (sizeLimit == 0)
        {
            // In earlier code, "size_limit" is taken instead of
            // "log_size_limit" as it said in the comment in AccessLogger.h.
            // In order to ensure backward compatibility we still take this
            // field as a fallback.
            sizeLimit = config.get("size_limit", 0).asUInt64();
        }
        if (sizeLimit > 0)
        {
            asyncFileLogger_.setFileSizeLimit(sizeLimit);
        }
    }
    drogon::app().registerPreSendingAdvice(
        [this](const drogon::HttpRequestPtr &req,
               const drogon::HttpResponsePtr &resp) {
            logging(LOG_RAW_TO(logIndex_), req, resp);
        });
}

void AccessLogger::shutdown()
{
}

void AccessLogger::logging(trantor::LogStream &stream,
                           const drogon::HttpRequestPtr &req,
                           const drogon::HttpResponsePtr &resp)
{
    for (auto &func : logFunctions_)
    {
        func(stream, req, resp);
    }
}

void AccessLogger::createLogFunctions(std::string format)
{
    std::string rawString;
    while (!format.empty())
    {
        LOG_INFO << format;
        auto pos = format.find('$');
        if (pos != std::string::npos)
        {
            rawString += format.substr(0, pos);

            format = format.substr(pos);
            std::regex e{"^\\$[a-zA-Z0-9\\-_]+"};
            std::smatch m;
            if (std::regex_search(format, m, e))
            {
                if (!rawString.empty())
                {
                    logFunctions_.emplace_back(
                        [rawString](trantor::LogStream &stream,
                                    const drogon::HttpRequestPtr &,
                                    const drogon::HttpResponsePtr &) {
                            stream << rawString;
                        });
                    rawString.clear();
                }
                auto placeholder = m[0];
                logFunctions_.emplace_back(newLogFunction(placeholder));
                format = m.suffix().str();
            }
            else
            {
                rawString += '$';
                format = format.substr(1);
            }
        }
        else
        {
            rawString += format;
            break;
        }
    }
    if (!rawString.empty())
    {
        logFunctions_.emplace_back(
            [rawString =
                 std::move(rawString)](trantor::LogStream &stream,
                                       const drogon::HttpRequestPtr &,
                                       const drogon::HttpResponsePtr &) {
                stream << rawString << "\n";
            });
    }
    else
    {
        logFunctions_.emplace_back(
            [](trantor::LogStream &stream,
               const drogon::HttpRequestPtr &,
               const drogon::HttpResponsePtr &) { stream << "\n"; });
    }
}

AccessLogger::LogFunction AccessLogger::newLogFunction(
    const std::string &placeholder)
{
    auto iter = logFunctionMap_.find(placeholder);
    if (iter != logFunctionMap_.end())
    {
        return iter->second;
    }
    if (placeholder.find("$http_") == 0 && placeholder.size() > 6)
    {
        auto headerName = placeholder.substr(6);
        return [headerName =
                    std::move(headerName)](trantor::LogStream &stream,
                                           const drogon::HttpRequestPtr &req,
                                           const drogon::HttpResponsePtr &) {
            outputReqHeader(stream, req, headerName);
        };
    }
    if (placeholder.find("$cookie_") == 0 && placeholder.size() > 8)
    {
        auto cookieName = placeholder.substr(8);
        return [cookieName =
                    std::move(cookieName)](trantor::LogStream &stream,
                                           const drogon::HttpRequestPtr &req,
                                           const drogon::HttpResponsePtr &) {
            outputReqCookie(stream, req, cookieName);
        };
    }
    if (placeholder.find("$upstream_http_") == 0 && placeholder.size() > 15)
    {
        auto headerName = placeholder.substr(15);
        return [headerName = std::move(
                    headerName)](trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &,
                                 const drogon::HttpResponsePtr &resp) {
            outputRespHeader(stream, resp, headerName);
        };
    }
    return [placeholder](trantor::LogStream &stream,
                         const drogon::HttpRequestPtr &,
                         const drogon::HttpResponsePtr &) {
        stream << placeholder;
    };
}

void AccessLogger::outputReqPath(trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &)
{
    stream << req->path();
}

void AccessLogger::outputDate(trantor::LogStream &stream,
                              const drogon::HttpRequestPtr &,
                              const drogon::HttpResponsePtr &) const
{
    if (useCustomTimeFormat_)
    {
        if (useLocalTime_)
        {
            stream << trantor::Date::now().toCustomedFormattedStringLocal(
                timeFormat_, showMicroseconds_);
        }
        else
        {
            stream << trantor::Date::now().toCustomedFormattedString(
                timeFormat_, showMicroseconds_);
        }
    }
    else
    {
        if (useLocalTime_)
        {
            stream << trantor::Date::now().toFormattedStringLocal(
                showMicroseconds_);
        }
        else
        {
            stream << trantor::Date::now().toFormattedString(showMicroseconds_);
        }
    }
}

void AccessLogger::outputReqDate(trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &) const
{
    if (useCustomTimeFormat_)
    {
        if (useLocalTime_)
        {
            stream << req->creationDate().toCustomedFormattedStringLocal(
                timeFormat_, showMicroseconds_);
        }
        else
        {
            stream << req->creationDate().toCustomedFormattedString(
                timeFormat_, showMicroseconds_);
        }
    }
    else
    {
        if (useLocalTime_)
        {
            stream << req->creationDate().toFormattedStringLocal(
                showMicroseconds_);
        }
        else
        {
            stream << req->creationDate().toFormattedString(showMicroseconds_);
        }
    }
}

//$request_query
void AccessLogger::outputReqQuery(trantor::LogStream &stream,
                                  const drogon::HttpRequestPtr &req,
                                  const drogon::HttpResponsePtr &)
{
    stream << req->query();
}
//$request_url
void AccessLogger::outputReqURL(trantor::LogStream &stream,
                                const drogon::HttpRequestPtr &req,
                                const drogon::HttpResponsePtr &)
{
    auto &query = req->query();
    if (query.empty())
    {
        stream << req->path();
    }
    else
    {
        stream << req->path() << '?' << query;
    }
}

//$request_version
void AccessLogger::outputVersion(trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &)
{
    stream << req->versionString();
}

//$request
void AccessLogger::outputReqLine(trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &)
{
    auto &query = req->query();
    if (query.empty())
    {
        stream << req->methodString() << " " << req->path() << " "
               << req->versionString();
    }
    else
    {
        stream << req->methodString() << " " << req->path() << '?' << query
               << " " << req->versionString();
    }
}

void AccessLogger::outputRemoteAddr(trantor::LogStream &stream,
                                    const drogon::HttpRequestPtr &req,
                                    const drogon::HttpResponsePtr &)
{
    stream << req->peerAddr().toIpPort();
}

void AccessLogger::outputLocalAddr(trantor::LogStream &stream,
                                   const drogon::HttpRequestPtr &req,
                                   const drogon::HttpResponsePtr &)
{
    stream << req->localAddr().toIpPort();
}

void AccessLogger::outputReqLength(trantor::LogStream &stream,
                                   const drogon::HttpRequestPtr &req,
                                   const drogon::HttpResponsePtr &)
{
    stream << req->body().length();
}

void AccessLogger::outputRespLength(trantor::LogStream &stream,
                                    const drogon::HttpRequestPtr &,
                                    const drogon::HttpResponsePtr &resp)
{
    stream << resp->body().length();
}
void AccessLogger::outputMethod(trantor::LogStream &stream,
                                const drogon::HttpRequestPtr &req,
                                const drogon::HttpResponsePtr &)
{
    stream << req->methodString();
}

void AccessLogger::outputThreadNumber(trantor::LogStream &stream,
                                      const drogon::HttpRequestPtr &,
                                      const drogon::HttpResponsePtr &)
{
#ifdef __linux__
    static thread_local pid_t threadId_{0};
#else
    static thread_local uint64_t threadId_{0};
#endif
#ifdef __linux__
    if (threadId_ == 0)
        threadId_ = static_cast<pid_t>(::syscall(SYS_gettid));
#elif defined __FreeBSD__
    if (threadId_ == 0)
    {
        threadId_ = pthread_getthreadid_np();
    }
#elif defined __OpenBSD__
    if (threadId_ == 0)
    {
        threadId_ = getthrid();
    }
#elif defined _WIN32 || defined __HAIKU__
    if (threadId_ == 0)
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        threadId_ = std::stoull(ss.str());
    }
#else
    if (threadId_ == 0)
    {
        pthread_threadid_np(NULL, &threadId_);
    }
#endif
    stream << threadId_;
}

//$http_[header_name]
void AccessLogger::outputReqHeader(trantor::LogStream &stream,
                                   const drogon::HttpRequestPtr &req,
                                   const std::string &headerName)
{
    stream << headerName << ": " << req->getHeader(headerName);
}

//$cookie_[cookie_name]
void AccessLogger::outputReqCookie(trantor::LogStream &stream,
                                   const drogon::HttpRequestPtr &req,
                                   const std::string &cookie)
{
    stream << "(cookie)" << cookie << "=" << req->getCookie(cookie);
}

//$upstream_http_[header_name]
void AccessLogger::outputRespHeader(trantor::LogStream &stream,
                                    const drogon::HttpResponsePtr &resp,
                                    const std::string &headerName)
{
    stream << headerName << ": " << resp->getHeader(headerName);
}

//$status
void AccessLogger::outputStatusString(trantor::LogStream &stream,
                                      const drogon::HttpRequestPtr &,
                                      const drogon::HttpResponsePtr &resp)
{
    int code = resp->getStatusCode();
    stream << code << " " << statusCodeToString(code);
}
//$status_code
void AccessLogger::outputStatusCode(trantor::LogStream &stream,
                                    const drogon::HttpRequestPtr &,
                                    const drogon::HttpResponsePtr &resp)
{
    stream << resp->getStatusCode();
}
//$processing_time
void AccessLogger::outputProcessingTime(trantor::LogStream &stream,
                                        const drogon::HttpRequestPtr &req,
                                        const drogon::HttpResponsePtr &)
{
    auto start = req->creationDate();
    auto end = trantor::Date::now();
    auto duration =
        end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
    auto seconds = static_cast<double>(duration) / 1000000.0;
    stream << seconds;
}

//$upstream_http_content-type $upstream_http_content_type
void AccessLogger::outputRespContentType(trantor::LogStream &stream,
                                         const drogon::HttpRequestPtr &,
                                         const drogon::HttpResponsePtr &resp)
{
    stream << resp->contentTypeString();
}
