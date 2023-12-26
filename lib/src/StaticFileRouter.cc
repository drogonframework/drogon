/**
 *
 *  StaticFileRouter.cc
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

#include "StaticFileRouter.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "RangeParser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/file.h>
#elif !defined(__MINGW32__)
#define stat _wstati64
#define S_ISREG(m) (((m)&0170000) == (0100000))
#define S_ISDIR(m) (((m)&0170000) == (0040000))
#endif
#include <sys/stat.h>
#include <filesystem>

using namespace drogon;

void StaticFileRouter::init(const std::vector<trantor::EventLoop *> &ioLoops)
{
    // Max timeout up to about 70 days;
    staticFilesCacheMap_ = std::make_unique<
        IOThreadStorage<std::unique_ptr<CacheMap<std::string, char>>>>();
    staticFilesCacheMap_->init(
        [&ioLoops](std::unique_ptr<CacheMap<std::string, char>> &mapPtr,
                   size_t i) {
            assert(i == ioLoops[i]->index());
            mapPtr = std::make_unique<CacheMap<std::string, char>>(ioLoops[i],
                                                                   1.0,
                                                                   4,
                                                                   50);
        });
    staticFilesCache_ = std::make_unique<
        IOThreadStorage<std::unordered_map<std::string, HttpResponsePtr>>>();
    ioLocationsPtr_ =
        std::make_shared<IOThreadStorage<std::vector<Location>>>();
    for (auto *loop : ioLoops)
    {
        loop->queueInLoop(
            [ioLocationsPtr = ioLocationsPtr_, locations = locations_] {
                **ioLocationsPtr = locations;
            });
    }
}

void StaticFileRouter::reset()
{
    staticFilesCacheMap_.reset();
    staticFilesCache_.reset();
    ioLocationsPtr_.reset();
    locations_.clear();
}

void StaticFileRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    const std::string &path = req->path();
    if (path.find("..") != std::string::npos)
    {
        auto directories = utils::splitString(path, "/");
        int traversalDepth = 0;
        for (const auto &dir : directories)
        {
            if (dir == "..")
            {
                traversalDepth--;
            }
            else if (dir != ".")
            {
                traversalDepth++;
            }

            if (traversalDepth < 0)
            {
                // Downloading files from the parent folder is forbidden.
                callback(app().getCustomErrorHandler()(k403Forbidden, req));
                return;
            }
        }
    }

    auto lPath = path;
    std::transform(lPath.begin(),
                   lPath.end(),
                   lPath.begin(),
                   [](unsigned char c) { return tolower(c); });

    for (auto &location : **ioLocationsPtr_)
    {
        auto &URI = location.uriPrefix_;
        if (location.realLocation_.empty())
        {
            if (!location.alias_.empty())
            {
                if (location.alias_[0] == '/')
                {
                    location.realLocation_ = location.alias_;
                }
                else
                {
                    location.realLocation_ =
                        HttpAppFrameworkImpl::instance().getDocumentRoot() +
                        location.alias_;
                }
            }
            else
            {
                location.realLocation_ =
                    HttpAppFrameworkImpl::instance().getDocumentRoot() +
                    location.uriPrefix_;
            }
            if (location.realLocation_[location.realLocation_.length() - 1] !=
                '/')
            {
                location.realLocation_.append(1, '/');
            }
            if (!location.isCaseSensitive_)
            {
                std::transform(URI.begin(),
                               URI.end(),
                               URI.begin(),
                               [](unsigned char c) { return tolower(c); });
            }
        }
        auto &tmpPath = location.isCaseSensitive_ ? path : lPath;
        if (tmpPath.length() >= URI.length() &&
            std::equal(tmpPath.begin(),
                       tmpPath.begin() + URI.length(),
                       URI.begin()))
        {
            std::string_view restOfThePath{path.data() + URI.length(),
                                           path.length() - URI.length()};
            auto pos = restOfThePath.rfind('/');
            if (pos != 0 && pos != std::string_view::npos &&
                !location.isRecursive_)
            {
                callback(app().getCustomErrorHandler()(k403Forbidden, req));
                return;
            }
            std::string filePath =
                location.realLocation_ +
                std::string{restOfThePath.data(), restOfThePath.length()};
            std::filesystem::path fsFilePath(utils::toNativePath(filePath));
            std::error_code err;
            if (!std::filesystem::exists(fsFilePath, err))
            {
                defaultHandler_(req, std::move(callback));
                return;
            }
            if (std::filesystem::is_directory(fsFilePath, err))
            {
                // Check if path is eligible for an implicit index.html
                if (implicitPageEnable_)
                {
                    filePath = filePath + "/" + implicitPage_;
                }
                else
                {
                    callback(app().getCustomErrorHandler()(k403Forbidden, req));
                    return;
                }
            }
            else
            {
                if (!location.allowAll_)
                {
                    pos = restOfThePath.rfind('.');
                    if (pos == std::string_view::npos)
                    {
                        callback(
                            app().getCustomErrorHandler()(k403Forbidden, req));
                        return;
                    }
                    std::string extension{restOfThePath.data() + pos + 1,
                                          restOfThePath.length() - pos - 1};
                    std::transform(extension.begin(),
                                   extension.end(),
                                   extension.begin(),
                                   [](unsigned char c) { return tolower(c); });
                    if (fileTypeSet_.find(extension) == fileTypeSet_.end())
                    {
                        callback(
                            app().getCustomErrorHandler()(k403Forbidden, req));
                        return;
                    }
                }
            }

            if (location.filters_.empty())
            {
                sendStaticFileResponse(filePath,
                                       req,
                                       std::move(callback),
                                       std::string_view{
                                           location.defaultContentType_});
            }
            else
            {
                filters_function::doFilters(
                    location.filters_,
                    req,
                    [this,
                     req,
                     filePath = std::move(filePath),
                     contentType =
                         std::string_view{location.defaultContentType_},
                     callback = std::move(callback)](
                        const HttpResponsePtr &resp) mutable {
                        if (resp)
                        {
                            callback(resp);
                        }
                        else
                        {
                            sendStaticFileResponse(filePath,
                                                   req,
                                                   std::move(callback),
                                                   contentType);
                        }
                    });
                return;
            }
        }
    }
    std::string directoryPath =
        HttpAppFrameworkImpl::instance().getDocumentRoot() + path;
    std::filesystem::path fsDirectoryPath(utils::toNativePath(directoryPath));
    std::error_code err;
    if (std::filesystem::exists(fsDirectoryPath, err))
    {
        if (std::filesystem::is_directory(fsDirectoryPath, err))
        {
            // Check if path is eligible for an implicit index.html
            if (implicitPageEnable_)
            {
                std::string filePath = directoryPath + "/" + implicitPage_;
                sendStaticFileResponse(filePath, req, std::move(callback), "");
                return;
            }
            else
            {
                callback(app().getCustomErrorHandler()(k403Forbidden, req));
                return;
            }
        }
        else
        {
            // This is a normal page
            auto pos = path.rfind('.');
            if (pos == std::string::npos)
            {
                callback(app().getCustomErrorHandler()(k403Forbidden, req));
                return;
            }
            std::string filetype = lPath.substr(pos + 1);
            if (fileTypeSet_.find(filetype) != fileTypeSet_.end())
            {
                // LOG_INFO << "file query!" << path;
                std::string filePath = directoryPath;
                sendStaticFileResponse(filePath, req, std::move(callback), "");
                return;
            }
        }
    }
    defaultHandler_(req, std::move(callback));
}

// Expand this struct as you need, nothing to worry about
struct FileStat
{
    size_t fileSize_;
    struct tm modifiedTime_;
    std::string modifiedTimeStr_;
};

// A wrapper to call stat()
// std::filesystem::file_time_type::clock::to_time_t still not
// implemented by M$, even in c++20, so keep calls to stat()
static bool getFileStat(const std::string &filePath, FileStat &myStat)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    struct _stati64 fileStat;
#else   // _WIN32
    struct stat fileStat;
#endif  // _WIN32
    if (stat(utils::toNativePath(filePath).c_str(), &fileStat) == 0 &&
        S_ISREG(fileStat.st_mode))
    {
        LOG_TRACE << "last modify time:" << fileStat.st_mtime;
#ifdef _WIN32
        gmtime_s(&myStat.modifiedTime_, &fileStat.st_mtime);
#else
        gmtime_r(&fileStat.st_mtime, &myStat.modifiedTime_);
#endif
        std::string &timeStr = myStat.modifiedTimeStr_;
        timeStr.resize(64);
        size_t len = strftime((char *)timeStr.data(),
                              timeStr.size(),
                              "%a, %d %b %Y %H:%M:%S GMT",
                              &myStat.modifiedTime_);
        timeStr.resize(len);

        myStat.fileSize_ = fileStat.st_size;
        return true;
    }

    return false;
}

void StaticFileRouter::sendStaticFileResponse(
    const std::string &filePath,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string_view &defaultContentType)
{
    if (req->method() != Get)
    {
        callback(app().getCustomErrorHandler()(k405MethodNotAllowed, req));
        return;
    }

    FileStat fileStat;
    bool fileExists = false;
    const std::string &rangeStr = req->getHeaderBy("range");
    if (enableRange_ && !rangeStr.empty())
    {
        if (!getFileStat(filePath, fileStat))
        {
            defaultHandler_(req, std::move(callback));
            return;
        }
        fileExists = true;
        // Check last modified time, rfc2616-14.25
        // If-Modified-Since: Mon, 15 Oct 2018 06:26:33 GMT
        // According to rfc 7233-3.1, preconditions must be evaluated before
        const std::string &modiStr = req->getHeaderBy("if-modified-since");
        if (enableLastModify_ && modiStr == fileStat.modifiedTimeStr_)
        {
            LOG_TRACE << "Not modified!";
            std::shared_ptr<HttpResponseImpl> resp =
                std::make_shared<HttpResponseImpl>();
            resp->setStatusCode(k304NotModified);
            resp->setContentTypeCode(CT_NONE);
            callback(resp);
            return;
        }
        // Check If-Range precondition
        const std::string &ifRange = req->getHeaderBy("if-range");
        if (ifRange.empty() || ifRange == fileStat.modifiedTimeStr_)
        {
            std::vector<FileRange> ranges;
            switch (parseRangeHeader(rangeStr, fileStat.fileSize_, ranges))
            {
                // TODO: support only single range now
                // Contributions are welcomed.
                case FileRangeParseResult::SinglePart:
                case FileRangeParseResult::MultiPart:
                {
                    auto firstRange = ranges.front();
                    auto ct = fileNameToContentTypeAndMime(filePath);
                    auto resp =
                        HttpResponse::newFileResponse(filePath,
                                                      firstRange.start,
                                                      firstRange.end -
                                                          firstRange.start,
                                                      true,
                                                      "",
                                                      ct.first,
                                                      std::string(ct.second),
                                                      req);
                    if (!fileStat.modifiedTimeStr_.empty())
                    {
                        resp->addHeader("Last-Modified",
                                        fileStat.modifiedTimeStr_);
                        resp->addHeader("Expires",
                                        "Thu, 01 Jan 1970 00:00:00 GMT");
                    }
                    callback(resp);
                    return;
                }
                case FileRangeParseResult::NotSatisfiable:
                {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k416RequestedRangeNotSatisfiable);
                    char buf[64];
                    snprintf(buf,
                             sizeof(buf),
                             "bytes */%zu",
                             fileStat.fileSize_);
                    resp->addHeader("Content-Range", std::string(buf));
                    callback(resp);
                    return;
                }
                /** rfc7233 4.4.
                 * > Note: Because servers are free to ignore Range, many
                 * implementations will simply respond with the entire selected
                 * representation in a 200 (OK) response.  That is partly
                 * because most clients are prepared to receive a 200 (OK) to
                 * complete the task (albeit less efficiently) and partly
                 * because clients might not stop making an invalid partial
                 * request until they have received a complete representation.
                 * Thus, clients cannot depend on receiving a 416 (Range Not
                 * Satisfiable) response even when it is most appropriate.
                 */
                default:
                    break;
            }
        }
    }

    // find cached response
    HttpResponsePtr cachedResp;
    auto &cacheMap = staticFilesCache_->getThreadData();
    auto iter = cacheMap.find(filePath);
    if (iter != cacheMap.end())
    {
        cachedResp = iter->second;
    }

    if (enableLastModify_)
    {
        if (cachedResp)
        {
            if (static_cast<HttpResponseImpl *>(cachedResp.get())
                    ->getHeaderBy("last-modified") ==
                req->getHeaderBy("if-modified-since"))
            {
                std::shared_ptr<HttpResponseImpl> resp =
                    std::make_shared<HttpResponseImpl>();
                resp->setStatusCode(k304NotModified);
                resp->setContentTypeCode(CT_NONE);
                callback(resp);
                return;
            }
        }
        else
        {
            LOG_TRACE << "enabled LastModify";
            if (!fileExists && !getFileStat(filePath, fileStat))
            {
                defaultHandler_(req, std::move(callback));
                return;
            }
            fileExists = true;
            const std::string &modiStr = req->getHeaderBy("if-modified-since");
            if (modiStr == fileStat.modifiedTimeStr_)
            {
                LOG_TRACE << "not Modified!";
                std::shared_ptr<HttpResponseImpl> resp =
                    std::make_shared<HttpResponseImpl>();
                resp->setStatusCode(k304NotModified);
                resp->setContentTypeCode(CT_NONE);
                callback(resp);
                return;
            }
        }
    }
    if (cachedResp)
    {
        LOG_TRACE << "Using file cache";
        callback(cachedResp);
        return;
    }
    // Check existence
    if (!fileExists)
    {
        std::filesystem::path fsFilePath(utils::toNativePath(filePath));
        std::error_code err;
        if (!std::filesystem::exists(fsFilePath, err) ||
            !std::filesystem::is_regular_file(fsFilePath, err))
        {
            defaultHandler_(req, std::move(callback));
            return;
        }
    }

    HttpResponsePtr resp;
    auto &acceptEncoding = req->getHeaderBy("accept-encoding");

    if (brStaticFlag_ && acceptEncoding.find("br") != std::string::npos)
    {
        // Find compressed file first.
        auto brFileName = filePath + ".br";
        std::filesystem::path fsBrFile(utils::toNativePath(brFileName));
        std::error_code err;
        if (std::filesystem::exists(fsBrFile, err) &&
            std::filesystem::is_regular_file(fsBrFile, err))
        {
            auto ct = fileNameToContentTypeAndMime(filePath);
            resp = HttpResponse::newFileResponse(
                brFileName, "", ct.first, std::string(ct.second), req);
            resp->addHeader("Content-Encoding", "br");
        }
    }
    if (!resp && gzipStaticFlag_ &&
        acceptEncoding.find("gzip") != std::string::npos)
    {
        // Find compressed file first.
        auto gzipFileName = filePath + ".gz";
        std::filesystem::path fsGzipFile(utils::toNativePath(gzipFileName));
        std::error_code err;
        if (std::filesystem::exists(fsGzipFile, err) &&
            std::filesystem::is_regular_file(fsGzipFile, err))
        {
            auto ct = fileNameToContentTypeAndMime(filePath);
            resp = HttpResponse::newFileResponse(
                gzipFileName, "", ct.first, std::string(ct.second), req);
            resp->addHeader("Content-Encoding", "gzip");
        }
    }
    if (!resp)
    {
        auto ct = fileNameToContentTypeAndMime(filePath);
        resp = HttpResponse::newFileResponse(
            filePath, "", ct.first, std::string(ct.second), req);
    }
    if (resp->statusCode() != k404NotFound)
    {
        if (resp->getContentType() == CT_APPLICATION_OCTET_STREAM &&
            !defaultContentType.empty())
        {
            resp->setContentTypeCodeAndCustomString(CT_CUSTOM,
                                                    defaultContentType);
        }
        if (!fileStat.modifiedTimeStr_.empty())
        {
            resp->addHeader("Last-Modified", fileStat.modifiedTimeStr_);
            resp->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
        }
        if (enableRange_)
        {
            resp->addHeader("accept-range", "bytes");
        }
        if (!headers_.empty())
        {
            for (auto &header : headers_)
            {
                resp->addHeader(header.first, header.second);
            }
        }
        // cache the response for 5 seconds by default
        if (staticFilesCacheTime_ >= 0)
        {
            LOG_TRACE << "Save in cache for " << staticFilesCacheTime_
                      << " seconds";
            resp->setExpiredTime(staticFilesCacheTime_);
            staticFilesCache_->getThreadData()[filePath] = resp;
            staticFilesCacheMap_->getThreadData()->insert(
                filePath, 0, staticFilesCacheTime_, [this, filePath]() {
                    LOG_TRACE << "Erase cache";
                    assert(staticFilesCache_->getThreadData().find(filePath) !=
                           staticFilesCache_->getThreadData().end());
                    staticFilesCache_->getThreadData().erase(filePath);
                });
        }
        callback(resp);
        return;
    }
    callback(resp);
}

void StaticFileRouter::setFileTypes(const std::vector<std::string> &types)
{
    fileTypeSet_.clear();
    for (auto const &type : types)
    {
        fileTypeSet_.insert(type);
    }
}

void StaticFileRouter::defaultHandler(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    callback(HttpResponse::newNotFoundResponse(req));
}
