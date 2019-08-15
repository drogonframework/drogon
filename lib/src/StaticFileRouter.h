/**
 *
 *  StaticFileRouter.h
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

#pragma once

#include "impl_forwards.h"
#include <drogon/CacheMap.h>
#include <functional>
#include <set>
#include <string>
#include <memory>

namespace drogon
{
class StaticFileRouter
{
  public:
    void route(const HttpRequestImplPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               bool needSetJsessionid,
               std::string &&sessionId);
    void setFileTypes(const std::vector<std::string> &types);
    void setStaticFilesCacheTime(int cacheTime)
    {
        _staticFilesCacheTime = cacheTime;
    }
    int staticFilesCacheTime() const
    {
        return _staticFilesCacheTime;
    }
    void setGzipStatic(bool useGzipStatic)
    {
        _gzipStaticFlag = useGzipStatic;
    }
    void init();

  private:
    std::set<std::string> _fileTypeSet = {"html",
                                          "js",
                                          "css",
                                          "xml",
                                          "xsl",
                                          "txt",
                                          "svg",
                                          "ttf",
                                          "otf",
                                          "woff2",
                                          "woff",
                                          "eot",
                                          "png",
                                          "jpg",
                                          "jpeg",
                                          "gif",
                                          "bmp",
                                          "ico",
                                          "icns"};

    std::unique_ptr<drogon::CacheMap<std::string, HttpResponsePtr>>
        _responseCachingMap;

    int _staticFilesCacheTime = 5;
    bool _enableLastModify = true;
    bool _gzipStaticFlag = true;
    std::unordered_map<std::string, std::weak_ptr<HttpResponse>>
        _staticFilesCache;
    std::mutex _staticFilesCacheMutex;
};
}  // namespace drogon