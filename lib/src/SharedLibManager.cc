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

#include "SharedLibManager.h"
#include <trantor/utils/Logger.h>

using namespace drogon;
SharedLibManager::SharedLibManager(trantor::EventLoop *loop,const std::string viewPath):
_loop(loop),
_viewPath(viewPath)
{
    _loop->runEvery(5.0,[=](){
        managerLibs();
    });
}
void SharedLibManager::managerLibs()
{
    //LOG_DEBUG<<"manager .so libs in path "<<_viewPath;
}