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
#include <drogon/IOThreadStorage.h>
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
               std::function<void(const HttpResponsePtr &)> &&callback);
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
    void init(const std::vector<trantor::EventLoop *> &ioloops);

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

    int _staticFilesCacheTime = 5;
    bool _enableLastModify = true;
    bool _gzipStaticFlag = true;
    std::unique_ptr<
        IOThreadStorage<std::unique_ptr<CacheMap<std::string, char>>>>
        _staticFilesCacheMap;
    std::unique_ptr<
        IOThreadStorage<std::unordered_map<std::string, HttpResponsePtr>>>
        _staticFilesCache;
};
}  // namespace drogon
