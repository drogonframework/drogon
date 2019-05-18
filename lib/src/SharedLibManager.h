/**
 *
 *  SharedLibManager.h
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

#include <trantor/net/EventLoop.h>
#include <trantor/utils/NonCopyable.h>
#include <unordered_map>
#include <vector>
namespace drogon
{
class SharedLibManager : public trantor::NonCopyable
{
  public:
    SharedLibManager(trantor::EventLoop *loop,
                     const std::vector<std::string> &libPaths);
    ~SharedLibManager();

  private:
    void managerLibs();
    trantor::EventLoop *_loop;
    std::vector<std::string> _libPaths;
    struct DLStat
    {
        void *handle = nullptr;
        struct timespec mTime = {0};
    };
    std::unordered_map<std::string, DLStat> _dlMap;
    void *loadLibs(const std::string &sourceFile, void *oldHld);
    trantor::TimerId _timeId;
};
}  // namespace drogon
