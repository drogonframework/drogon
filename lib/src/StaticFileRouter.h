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
        staticFilesCacheTime_ = cacheTime;
    }
    int staticFilesCacheTime() const
    {
        return staticFilesCacheTime_;
    }
    void setGzipStatic(bool useGzipStatic)
    {
        gzipStaticFlag_ = useGzipStatic;
    }
    void init(const std::vector<trantor::EventLoop *> &ioloops);
    StaticFileRouter(
        const std::vector<std::pair<std::string, std::string>> &headers)
        : headers_(headers)
    {
    }

  private:
    std::set<std::string> fileTypeSet_{"html",
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

    int staticFilesCacheTime_{5};
    bool enableLastModify_{true};
    bool gzipStaticFlag_{true};
    std::unique_ptr<
        IOThreadStorage<std::unique_ptr<CacheMap<std::string, char>>>>
        staticFilesCacheMap_;
    std::unique_ptr<
        IOThreadStorage<std::unordered_map<std::string, HttpResponsePtr>>>
        staticFilesCache_;
    const std::vector<std::pair<std::string, std::string>> &headers_;
};
}  // namespace drogon
