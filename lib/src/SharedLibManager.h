/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */
#pragma once

#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <unordered_map>
namespace drogon{
    class SharedLibManager:public trantor::NonCopyable
    {
    public:
        SharedLibManager(trantor::EventLoop *loop,const std::string viewPath);
        ~SharedLibManager(){}
    private:
        void managerLibs();
        trantor::EventLoop *_loop;
        std::string _viewPath;
        std::unordered_map<std::string,void *> _dlMap;
    };
}