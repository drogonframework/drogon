/**
 *
 *  CacheFile.cc
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

#include "CacheFile.h"
#include <trantor/utils/Logger.h>
#ifdef _WIN32
#include <mman.h>
#include <drogon/utils/Utilities.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

using namespace drogon;

CacheFile::CacheFile(const std::string &path, bool autoDelete)
    : autoDelete_(autoDelete), path_(path)
{
#ifndef _MSC_VER
    file_ = fopen(path_.data(), "wb+");
#else
    auto wPath{drogon::utils::toNativePath(path)};
    if (_wfopen_s(&file_, wPath.c_str(), L"wb+") != 0)
    {
        file_ = nullptr;
    }
#endif
    if (!file_)
        LOG_SYSERR << "CacheFile fopen:";
}

CacheFile::~CacheFile()
{
    if (data_)
    {
        munmap(data_, dataLength_);
    }
    if (autoDelete_ && file_)
    {
        fclose(file_);
#if defined(_WIN32) && !defined(__MINGW32__)
        auto wPath{drogon::utils::toNativePath(path_)};
        _wunlink(wPath.c_str());
#else
        unlink(path_.data());
#endif
    }
    else if (file_)
    {
        fclose(file_);
    }
}

void CacheFile::append(const char *data, size_t length)
{
    if (file_)
    {
        if (!fwrite(data, length, 1, file_))
            LOG_SYSERR << "CacheFile append:";
    }
}

size_t CacheFile::length()
{
    if (file_)
#ifdef _WIN32
        return _ftelli64(file_);
#else
        return ftell(file_);
#endif
    return 0;
}

char *CacheFile::data()
{
    if (!file_)
        return nullptr;
    if (!data_)
    {
        fflush(file_);
#ifdef _WIN32
        auto fd = _fileno(file_);
#else
        auto fd = fileno(file_);
#endif
        dataLength_ = length();
        data_ = static_cast<char *>(
            mmap(nullptr, dataLength_, PROT_READ, MAP_SHARED, fd, 0));
        if (data_ == MAP_FAILED)
        {
            data_ = nullptr;
            LOG_SYSERR << "CacheFile mmap:";
        }
    }
    return data_;
}
