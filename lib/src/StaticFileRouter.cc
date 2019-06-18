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

#include <fstream>
#include <iostream>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

using namespace drogon;

void StaticFileRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    bool needSetJsessionid,
    std::string &&sessionId)
{
    const std::string &path = req->path();
    auto pos = path.rfind(".");
    if (pos != std::string::npos)
    {
        std::string filetype = path.substr(pos + 1);
        transform(filetype.begin(), filetype.end(), filetype.begin(), tolower);
        if (_fileTypeSet.find(filetype) != _fileTypeSet.end())
        {
            // LOG_INFO << "file query!" << path;
            std::string filePath = drogon::app().getDocumentRoot() + path;
            if (filePath.find("/../") != std::string::npos)
            {
                // Downloading files from the parent folder is forbidden.
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k403Forbidden);
                callback(resp);
                return;
            }
            // find cached response
            HttpResponsePtr cachedResp;
            {
                std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
                if (_staticFilesCache.find(filePath) != _staticFilesCache.end())
                {
                    cachedResp = _staticFilesCache[filePath].lock();
                    if (!cachedResp)
                    {
                        _staticFilesCache.erase(filePath);
                    }
                }
            }

            // check last modified time,rfc2616-14.25
            // If-Modified-Since: Mon, 15 Oct 2018 06:26:33 GMT

            std::string timeStr;
            if (_enableLastModify)
            {
                if (cachedResp)
                {
                    if (std::dynamic_pointer_cast<HttpResponseImpl>(cachedResp)
                            ->getHeaderBy("last-modified") ==
                        req->getHeaderBy("if-modified-since"))
                    {
                        std::shared_ptr<HttpResponseImpl> resp =
                            std::make_shared<HttpResponseImpl>();
                        resp->setStatusCode(k304NotModified);
                        if (needSetJsessionid)
                        {
                            resp->addCookie("JSESSIONID", sessionId);
                        }
                        callback(resp);
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
                            if (needSetJsessionid)
                            {
                                resp->addCookie("JSESSIONID", sessionId);
                            }
                            callback(resp);
                            return;
                        }
                    }
                }
            }

            if (cachedResp)
            {
                if (needSetJsessionid)
                {
                    // make a copy
                    auto newCachedResp = std::make_shared<HttpResponseImpl>(
                        *std::dynamic_pointer_cast<HttpResponseImpl>(
                            cachedResp));
                    newCachedResp->addCookie("JSESSIONID", sessionId);
                    newCachedResp->setExpiredTime(-1);
                    callback(newCachedResp);
                    return;
                }
                callback(cachedResp);
                return;
            }
            HttpResponsePtr resp;
            if (_gzipStaticFlag &&
                req->getHeaderBy("accept-encoding").find("gzip") !=
                    std::string::npos)
            {
                // Find compressed file first.
                auto gzipFileName = filePath + ".gz";
                std::ifstream infile(gzipFileName, std::ifstream::binary);
                if (infile)
                {
                    resp = HttpResponse::newFileResponse(
                        gzipFileName, "", drogon::getContentType(filePath));
                    resp->addHeader("Content-Encoding", "gzip");
                }
            }
            if (!resp)
                resp = HttpResponse::newFileResponse(filePath);
            if (resp->statusCode() != k404NotFound)
            {
                if (!timeStr.empty())
                {
                    resp->addHeader("Last-Modified", timeStr);
                    resp->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
                }
                // cache the response for 5 seconds by default
                if (_staticFilesCacheTime >= 0)
                {
                    resp->setExpiredTime(_staticFilesCacheTime);
                    _responseCachingMap->insert(
                        filePath, resp, resp->expiredTime(), [=]() {
                            std::lock_guard<std::mutex> guard(
                                _staticFilesCacheMutex);
                            _staticFilesCache.erase(filePath);
                        });
                    {
                        std::lock_guard<std::mutex> guard(
                            _staticFilesCacheMutex);
                        _staticFilesCache[filePath] = resp;
                    }
                }

                if (needSetJsessionid)
                {
                    auto newCachedResp = resp;
                    if (resp->expiredTime() >= 0)
                    {
                        // make a copy
                        newCachedResp = std::make_shared<HttpResponseImpl>(
                            *std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                        newCachedResp->setExpiredTime(-1);
                    }
                    newCachedResp->addCookie("JSESSIONID", sessionId);
                    callback(newCachedResp);
                    return;
                }
            }
            callback(resp);
            return;
        }
    }

    callback(HttpResponse::newNotFoundResponse());
}

void StaticFileRouter::setFileTypes(const std::vector<std::string> &types)
{
    _fileTypeSet.clear();
    for (auto const &type : types)
    {
        _fileTypeSet.insert(type);
    }
}