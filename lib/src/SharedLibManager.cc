/**
 *
 *  @file SharedLibManager.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "SharedLibManager.h"
#include <drogon/config.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
#include <sys/types.h>
#include <trantor/utils/Logger.h>
#include <unistd.h>
static void forEachFileIn(
    const std::string &path,
    const std::function<void(const std::string &, const struct stat &)> &cb)
{
    DIR *dp;
    struct dirent *dirp;
    struct stat st;

    /* open dirent directory */
    if ((dp = opendir(path.c_str())) == NULL)
    {
        // perror("opendir:");
        LOG_ERROR << "can't open dir,path:" << path;
        return;
    }

    /**
     * read all files in this dir
     **/
    while ((dirp = readdir(dp)) != NULL)
    {
        /* ignore hidden files */
        if (dirp->d_name[0] == '.')
            continue;
        /* get dirent status */
        std::string filename = dirp->d_name;
        std::string fullname = path;
        fullname.append("/").append(filename);
        if (stat(fullname.c_str(), &st) == -1)
        {
            perror("stat");
            closedir(dp);
            return;
        }

        /* if dirent is a directory, find files recursively */
        if (S_ISDIR(st.st_mode))
        {
            forEachFileIn(fullname, cb);
        }
        else
        {
            cb(fullname, st);
        }
    }
    closedir(dp);
    return;
}

using namespace drogon;
SharedLibManager::SharedLibManager(const std::vector<std::string> &libPaths,
                                   const std::string &outputPath)
    : libPaths_(libPaths), outputPath_(outputPath)
{
    workingThread_.run();
    timeId_ =
        workingThread_.getLoop()->runEvery(5.0, [this]() { managerLibs(); });
}
SharedLibManager::~SharedLibManager()
{
    workingThread_.getLoop()->invalidateTimer(timeId_);
}
void SharedLibManager::managerLibs()
{
    for (auto const &libPath : libPaths_)
    {
        forEachFileIn(
            libPath,
            [this, libPath](const std::string &filename,
                            const struct stat &st) {
                auto pos = filename.rfind('.');
                if (pos != std::string::npos)
                {
                    auto exName = filename.substr(pos + 1);
                    if (exName == "csp")
                    {
                        // compile
                        auto lockFile = filename + ".lock";
                        std::ifstream fin(lockFile);
                        if (fin)
                        {
                            return;
                        }

                        void *oldHandle = nullptr;
                        if (dlMap_.find(filename) != dlMap_.end())
                        {
#ifdef __linux__
                            if (st.st_mtim.tv_sec >
                                dlMap_[filename].mTime.tv_sec)
#elif defined _WIN32
                            if (st.st_mtime > dlMap_[filename].mTime.tv_sec)
#else
                            if (st.st_mtimespec.tv_sec >
                                dlMap_[filename].mTime.tv_sec)
#endif
                            {
                                LOG_TRACE << "new csp file:" << filename;
                                oldHandle = dlMap_[filename].handle;
                            }
                            else
                                return;
                        }

                        {
                            std::ofstream fout(lockFile);
                        }

                        auto srcFile = filename.substr(0, pos);
                        if (!outputPath_.empty())
                        {
                            pos = srcFile.rfind("/");
                            if (pos != std::string::npos)
                            {
                                srcFile = srcFile.substr(pos + 1);
                            }
                            srcFile = outputPath_ + "/" + srcFile;
                        }
                        auto soFile = srcFile + ".so";
                        DLStat dlStat;
                        if (!shouldCompileLib(soFile, st))
                        {
                            LOG_TRACE << "Using already compiled library:"
                                      << soFile;
                            dlStat.handle = loadLib(soFile, oldHandle);
                        }
                        else
                        {
                            // generate source code and compile it.
                            std::string cmd = "drogon_ctl create view ";
                            if (!outputPath_.empty())
                            {
                                cmd.append(filename).append(" -o ").append(
                                    outputPath_);
                            }
                            else
                            {
                                cmd.append(filename).append(" -o ").append(
                                    libPath);
                            }
                            srcFile.append(".cc");
                            LOG_TRACE << cmd;
                            auto r = system(cmd.c_str());
                            // TODO: handle r
                            (void)(r);
                            dlStat.handle =
                                compileAndLoadLib(srcFile, oldHandle);
                        }
#ifdef __linux__
                        dlStat.mTime = st.st_mtim;
#elif defined _WIN32
                        dlStat.mTime.tv_sec = st.st_mtime;
#else
                        dlStat.mTime = st.st_mtimespec;
#endif
                        if (dlStat.handle)
                        {
                            dlMap_[filename] = dlStat;
                        }
                        else
                        {
                            dlStat.handle = dlMap_[filename].handle;
                            dlMap_[filename] = dlStat;
                        }
                        workingThread_.getLoop()->runAfter(3.5, [lockFile]() {
                            LOG_TRACE << "remove file " << lockFile;
                            if (unlink(lockFile.c_str()) == -1)
                                perror("");
                        });
                    }
                }
            });
    }
}

void *SharedLibManager::compileAndLoadLib(const std::string &sourceFile,
                                          void *oldHld)
{
    LOG_TRACE << "src:" << sourceFile;
    std::string cmd = COMPILER_COMMAND;
    auto pos = cmd.rfind('/');
    if (pos != std::string::npos)
    {
        cmd = cmd.substr(pos + 1);
    }
    cmd.append(" ")
        .append(sourceFile)
        .append(" ")
        .append(COMPILATION_FLAGS)
        .append(" ")
        .append(INCLUDING_DIRS);
    if (std::string(COMPILER_ID).find("Clang") != std::string::npos)
        cmd.append(" -shared -fPIC -undefined dynamic_lookup -o ");
    else
        cmd.append(" -shared -fPIC --no-gnu-unique -o ");
    pos = sourceFile.rfind('.');
    auto soFile = sourceFile.substr(0, pos);
    soFile.append(".so");
    cmd.append(soFile);
    LOG_TRACE << cmd;

    if (system(cmd.c_str()) == 0)
    {
        LOG_TRACE << "Compiled successfully:" << soFile;
        return loadLib(soFile, oldHld);
    }
    else
    {
        LOG_DEBUG << "Could not compile library.";
        return nullptr;
    }
}

bool SharedLibManager::shouldCompileLib(const std::string &soFile,
                                        const struct stat &sourceStat)
{
#ifdef __linux__
    auto sourceModifiedTime = sourceStat.st_mtim.tv_sec;
#elif defined _WIN32
    auto sourceModifiedTime = sourceStat.st_mtime;
#else
    auto sourceModifiedTime = sourceStat.st_mtimespec.tv_sec;
#endif

    struct stat soStat;
    if (stat(soFile.c_str(), &soStat) == -1)
    {
        LOG_TRACE << "Cannot determine modification time for:" << soFile;
        return true;
    }

#ifdef __linux__
    auto soModifiedTime = soStat.st_mtim.tv_sec;
#elif defined _WIN32
    auto soModifiedTime = soStat.st_mtime;
#else
    auto soModifiedTime = soStat.st_mtimespec.tv_sec;
#endif

    return (sourceModifiedTime > soModifiedTime);
}

void *SharedLibManager::loadLib(const std::string &soFile, void *oldHld)
{
    if (oldHld)
    {
        if (dlclose(oldHld) == 0)
        {
            LOG_TRACE << "Successfully closed dynamic library:" << oldHld;
        }
        else
        {
            LOG_TRACE << dlerror();
        }
    }
    auto Handle = dlopen(soFile.c_str(), RTLD_LAZY);
    if (!Handle)
    {
        LOG_ERROR << "load " << soFile << " error!";
        LOG_ERROR << dlerror();
    }
    else
    {
        LOG_TRACE << "Successfully loaded library file " << soFile;
    }

    return Handle;
}
