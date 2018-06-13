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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static void forEachFileIn(const std::string &path,const std::function<void (const std::string &filename)> &cb)
{
    DIR* dp;
    struct dirent* dirp;
    struct stat st;

    /* open dirent directory */
    if((dp = opendir(path.c_str())) == NULL)
    {
        perror("opendir");
        return;
    }

    /**
     * read all files in this dir
     **/
    while((dirp = readdir(dp)) != NULL)
    {
        /* ignore hidden files */
        if(dirp->d_name[0] == '.')
            continue;
        /* get dirent status */
        std::string filename=dirp->d_name;
        std::string fullname=path;
        fullname.append("/").append(filename);
        if(stat(fullname.c_str(), &st) == -1)
        {
            perror("stat");
            return;
        }

        /* if dirent is a directory, continue */
        if(S_ISDIR(st.st_mode))
            continue;
        cb(dirp->d_name);

    }
    return;
}

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
    forEachFileIn("./",[](const std::string &filename){
        LOG_DEBUG<<filename;
    });
}