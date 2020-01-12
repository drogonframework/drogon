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
#include <fstream>
#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

using namespace drogon;

void StaticFileRouter::init(const std::vector<trantor::EventLoop *> &ioloops)
{
    // Max timeout up to about 70 days;
    staticFilesCacheMap_ = decltype(staticFilesCacheMap_)(
        new IOThreadStorage<std::unique_ptr<CacheMap<std::string, char>>>);
    staticFilesCacheMap_->init(
        [&ioloops](std::unique_ptr<CacheMap<std::string, char>> &mapPtr,
                   size_t i) {
            assert(i == ioloops[i]->index());
            mapPtr = std::unique_ptr<CacheMap<std::string, char>>(
                new CacheMap<std::string, char>(ioloops[i], 1.0, 4, 50));
        });
    staticFilesCache_ = decltype(staticFilesCache_)(
        new IOThreadStorage<
            std::unordered_map<std::string, HttpResponsePtr>>{});
    ioLocationsPtr_ =
        decltype(ioLocationsPtr_)(new IOThreadStorage<std::vector<Location>>);
    for (auto *loop : ioloops)
    {
        loop->queueInLoop([this] { **ioLocationsPtr_ = locations_; });
    }
}

void StaticFileRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    const std::string &path = req->path();
    if (path.find("/../") != std::string::npos)
    {
        // Downloading files from the parent folder is forbidden.
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }
    auto lPath = path;
    std::transform(lPath.begin(), lPath.end(), lPath.begin(), tolower);

    for (auto &location : **ioLocationsPtr_)
    {
        auto &URI = location.uriPrefix_;
        auto &defaultContentType = location.defaultContentType_;
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
                std::transform(URI.begin(), URI.end(), URI.begin(), tolower);
            }
        }
        auto &tmpPath = location.isCaseSensitive_ ? path : lPath;
        if (tmpPath.length() >= URI.length() &&
            std::equal(tmpPath.begin(),
                       tmpPath.begin() + URI.length(),
                       URI.begin()))
        {
            string_view restOfThePath{path.data() + URI.length(),
                                      path.length() - URI.length()};
            auto pos = restOfThePath.find('/');
            if (pos != 0 && pos != string_view::npos && !location.isRecursive_)
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k403Forbidden);
                callback(resp);
                return;
            }
            if (!location.allowAll_)
            {
                pos = restOfThePath.rfind('.');
                if (pos == string_view::npos)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k403Forbidden);
                    callback(resp);
                    return;
                }
                std::string extension{restOfThePath.data() + pos + 1,
                                      restOfThePath.length() - pos - 1};
                std::transform(extension.begin(),
                               extension.end(),
                               extension.begin(),
                               tolower);
                if (fileTypeSet_.find(extension) == fileTypeSet_.end())
                {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k403Forbidden);
                    callback(resp);
                    return;
                }
            }
            std::string filePath =
                location.realLocation_ +
                std::string{restOfThePath.data(), restOfThePath.length()};
            sendStaticFileResponse(filePath,
                                   req,
                                   callback,
                                   string_view{location.defaultContentType_});
            return;
        }
    }
    auto pos = lPath.rfind('.');
    if (pos != std::string::npos)
    {
        std::string filetype = lPath.substr(pos + 1);
        if (fileTypeSet_.find(filetype) != fileTypeSet_.end())
        {
            // LOG_INFO << "file query!" << path;
            std::string filePath =
                HttpAppFrameworkImpl::instance().getDocumentRoot() + path;
            sendStaticFileResponse(filePath, req, callback, "");
            return;
        }
    }

    callback(HttpResponse::newNotFoundResponse());
}

void StaticFileRouter::sendStaticFileResponse(
    const std::string &filePath,
    const HttpRequestImplPtr &req,
    const std::function<void(const HttpResponsePtr &)> &callback,
    const string_view &defaultContentType)
{  // find cached response
    HttpResponsePtr cachedResp;
    auto &cacheMap = staticFilesCache_->getThreadData();
    auto iter = cacheMap.find(filePath);
    if (iter != cacheMap.end())
    {
        cachedResp = iter->second;
    }

    // check last modified time,rfc2616-14.25
    // If-Modified-Since: Mon, 15 Oct 2018 06:26:33 GMT

    std::string timeStr;
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
                HttpAppFrameworkImpl::instance().callCallback(req,
                                                              resp,
                                                              callback);
                return;
            }
        }
        else
        {
            struct stat fileStat;
            LOG_TRACE << "enabled LastModify";
            if (stat(filePath.c_str(), &fileStat) >= 0)
            {
                LOG_TRACE << "last modify time:" << fileStat.st_mtime;
                struct tm tm1;
                gmtime_r(&fileStat.st_mtime, &tm1);
                timeStr.resize(64);
                auto len = strftime((char *)timeStr.data(),
                                    timeStr.size(),
                                    "%a, %d %b %Y %T GMT",
                                    &tm1);
                timeStr.resize(len);
                const std::string &modiStr =
                    req->getHeaderBy("if-modified-since");
                if (modiStr == timeStr && !modiStr.empty())
                {
                    LOG_TRACE << "not Modified!";
                    std::shared_ptr<HttpResponseImpl> resp =
                        std::make_shared<HttpResponseImpl>();
                    resp->setStatusCode(k304NotModified);
                    HttpAppFrameworkImpl::instance().callCallback(req,
                                                                  resp,
                                                                  callback);
                    return;
                }
            }
        }
    }

    if (cachedResp)
    {
        LOG_TRACE << "Using file cache";
        HttpAppFrameworkImpl::instance().callCallback(req,
                                                      cachedResp,
                                                      callback);
        return;
    }
    HttpResponsePtr resp;
    if (gzipStaticFlag_ &&
        req->getHeaderBy("accept-encoding").find("gzip") != std::string::npos)
    {
        // Find compressed file first.
        auto gzipFileName = filePath + ".gz";
        std::ifstream infile(gzipFileName, std::ifstream::binary);
        if (infile)
        {
            resp =
                HttpResponse::newFileResponse(gzipFileName,
                                              "",
                                              drogon::getContentType(filePath));
            resp->addHeader("Content-Encoding", "gzip");
        }
    }
    if (!resp)
        resp = HttpResponse::newFileResponse(filePath);
    if (resp->statusCode() != k404NotFound)
    {
        if (resp->getContentType() == CT_APPLICATION_OCTET_STREAM &&
            !defaultContentType.empty())
        {
            resp->setContentTypeCodeAndCustomString(CT_CUSTOM,
                                                    defaultContentType);
        }
        if (!timeStr.empty())
        {
            resp->addHeader("Last-Modified", timeStr);
            resp->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
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
        HttpAppFrameworkImpl::instance().callCallback(req, resp, callback);
        return;
    }
    callback(resp);
    return;
}
void StaticFileRouter::setFileTypes(const std::vector<std::string> &types)
{
    fileTypeSet_.clear();
    for (auto const &type : types)
    {
        fileTypeSet_.insert(type);
    }
}
