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

#include <drogon/drogon.h>
#include <drogon/plugins/AccessLogger.h>
#include <regex>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#include <sys/syscall.h>
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
    logFunctionMap_ = {{"$request_path", outputReqPath},
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
                       {"$remote_addr", outputRemoteAddr},
                       {"$local_addr", outputLocalAddr},
                       {"$request_len", outputReqLength},
                       {"$method", outputMethod},
                       {"$thread", outputThreadNumber}};
    auto format = config.get("log_format", "$request_url").asString();
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
        auto sizeLimit = config.get("size_limit", 0).asUInt64();
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
            std::regex e{"^\\$[a-z0-9\\-_]+"};
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
                              const drogon::HttpResponsePtr &)
{
    if (useLocalTime_)
    {
        stream << trantor::Date::now().toFormattedStringLocal(true);
    }
    else
    {
        stream << trantor::Date::now().toFormattedString(true);
    }
}

void AccessLogger::outputReqDate(trantor::LogStream &stream,
                                 const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &)
{
    if (useLocalTime_)
    {
        stream << req->creationDate().toFormattedStringLocal(true);
    }
    else
    {
        stream << req->creationDate().toFormattedString(true);
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
#elif defined _WIN32
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