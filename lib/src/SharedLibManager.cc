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
#include <drogon/config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
static void forEachFileIn(const std::string &path,const std::function<void (const std::string &,const struct stat &)> &cb)
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
        cb(fullname,st);

    }
    return;
}

using namespace drogon;
SharedLibManager::SharedLibManager(trantor::EventLoop *loop,const std::vector<std::string> & libPaths):
_loop(loop),
_libPaths(libPaths)
{
    _loop->runEvery(5.0,[=](){
        managerLibs();
    });
}
void SharedLibManager::managerLibs()
{
    for(auto libPath:_libPaths)
    {
        forEachFileIn(libPath,[=](const std::string &filename,const struct stat &st){
            //LOG_DEBUG<<filename;
            auto pos=filename.rfind(".");
            if(pos!=std::string::npos)
            {
                auto exName=filename.substr(pos+1);
                if(exName=="csp")
                {
                    //compile
                    auto lockFile=filename+".lock";
                    std::ifstream fin(lockFile);
                    if (fin) {
                        return;
                    }

                    void *oldHandle= nullptr;
                    if(_dlMap.find(filename)!=_dlMap.end())
                    {
#ifdef __linux__
                        if(st.st_mtim.tv_sec>_dlMap[filename].mTime.tv_sec)
#else
                        if(st.st_mtimespec.tv_sec>_dlMap[filename].mTime.tv_sec)
#endif
                        {
                            LOG_DEBUG<<"new csp file:"<<filename;
                            oldHandle=_dlMap[filename].handle;
                        }
                        else
                            return;
                    }

                    {
                        std::ofstream fout(lockFile);
                    }
                    std::string cmd="drogon_ctl create view ";
                    cmd.append(filename).append(" -o ").append(libPath);
                    LOG_DEBUG<<cmd;
                    system(cmd.c_str());
                    auto srcFile=filename.substr(0,pos);
                    srcFile.append(".cc");
                    DLStat dlStat;
                    dlStat.handle=loadLibs(srcFile,oldHandle);
#ifdef __linux__
                    dlStat.mTime=st.st_mtim;
#else
                    dlStat.mTime=st.st_mtimespec;
#endif
                    if(dlStat.handle)
                    {
                        _dlMap[filename]=dlStat;
                    }
                    else
                    {
                        dlStat.handle=_dlMap[filename].handle;
                        _dlMap[filename]=dlStat;
                    }
                    _loop->runAfter(3.5,[=](){
                        LOG_DEBUG<<"remove file "<<lockFile;
                        if(unlink(lockFile.c_str())==-1)
                            perror("");
                    });

                }
            }
        });
    }

}
void* SharedLibManager::loadLibs(const std::string &sourceFile,void *oldHld) {
    LOG_DEBUG<<"src:"<<sourceFile;
    std::string cmd="g++ ";
    cmd.append(sourceFile).append(" ")
            .append(compileFlags).append(includeDirs).append(" -shared -fPIC --no-gnu-unique -o ");
    auto pos=sourceFile.rfind(".");
    auto soFile=sourceFile.substr(0,pos);
    soFile.append(".so");
    cmd.append(soFile);
    void *Handle = nullptr;
    if(system(cmd.c_str())==0)
    {
        LOG_DEBUG<<"Compiled successfully";
        if(oldHld)
        {
            if(dlclose(oldHld)==0)
            {
                LOG_DEBUG<<"close dynamic lib successfully:"<<oldHld;
            }
            else
            {
                LOG_DEBUG<<dlerror();
            }
        }

        //loading so file;
        Handle=dlopen(soFile.c_str(),RTLD_LAZY);
        if(!Handle)
        {
            LOG_ERROR<<"load "<<soFile<<" error!";
            LOG_ERROR<<dlerror();
        }
        else
        {
            LOG_DEBUG<<"Successfully loaded library file "<<soFile;
        }
    }
    return Handle;
}
