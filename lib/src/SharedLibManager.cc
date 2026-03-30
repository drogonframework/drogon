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
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <trantor/utils/Logger.h>
#include <unistd.h>

// Safe exec helper: runs a program with explicit argv, no shell involved.
// Returns the exit status, or -1 on fork/exec failure.
static int safeExec(const std::vector<std::string> &args)
{
    if (args.empty())
        return -1;

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (auto &a : args)
        argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (pid == 0)
    {
        // Child: replace image with the target program.
        execvp(argv[0], argv.data());
        // execvp only returns on error.
        perror("execvp");
        _exit(127);
    }
    // Parent: wait for child.
    int status = 0;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid");
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

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
#if defined __linux__ || defined __HAIKU__
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
                            const std::string &outDir =
                                !outputPath_.empty() ? outputPath_ : libPath;
                            std::vector<std::string> genArgs = {"drogon_ctl",
                                                                "create",
                                                                "view",
                                                                filename,
                                                                "-o",
                                                                outDir};
                            srcFile.append(".cc");
                            LOG_TRACE << "drogon_ctl create view " << filename
                                      << " -o " << outDir;
                            auto r = safeExec(genArgs);
                            if (r != 0)
                            {
                                LOG_ERROR << "Failed to generate source code for "
                                          << filename;
                                
                                dlStat.handle = oldHandle;
                                return;
                            }
                            dlStat.handle = compileAndLoadLib(srcFile, oldHandle);
                        }
#if defined __linux__ || defined __HAIKU__
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
    auto pos = sourceFile.rfind('.');
    auto soFile = sourceFile.substr(0, pos);
    soFile.append(".so");

    // Build argv without invoking a shell so that metacharacters in
    // sourceFile or soFile cannot be interpreted by /bin/sh.
    std::vector<std::string> compileArgs;
    compileArgs.push_back(COMPILER_COMMAND);

    // COMPILATION_FLAGS and INCLUDING_DIRS are baked in at build time from
    // trusted CMake variables; split them on whitespace into separate tokens.
    auto splitIntoArgs = [&](const std::string &s) {
        std::istringstream iss(s);
        std::string token;
        while (iss >> token)
            compileArgs.push_back(token);
    };
    compileArgs.push_back(sourceFile);
    splitIntoArgs(COMPILATION_FLAGS);
    splitIntoArgs(INCLUDING_DIRS);
    if (std::string(COMPILER_ID).find("Clang") != std::string::npos)
    {
        compileArgs.push_back("-shared");
        compileArgs.push_back("-fPIC");
        compileArgs.push_back("-undefined");
        compileArgs.push_back("dynamic_lookup");
    }
    else
    {
        compileArgs.push_back("-shared");
        compileArgs.push_back("-fPIC");
        compileArgs.push_back("--no-gnu-unique");
    }
    compileArgs.push_back("-o");
    compileArgs.push_back(soFile);

    LOG_TRACE << COMPILER_COMMAND << " " << sourceFile << " ... -o " << soFile;

    if (safeExec(compileArgs) == 0)
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
#if defined __linux__ || defined __HAIKU__
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

#if defined __linux__ || defined __HAIKU__
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
