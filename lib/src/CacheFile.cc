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
#include <unistd.h>
#include <sys/mman.h>

using namespace drogon;

CacheFile::CacheFile(const std::string &path, bool autoDelete)
    : autoDelete_(autoDelete), path_(path)
{
    file_ = fopen(path_.data(), "w+");
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
        unlink(path_.data());
    }
    else if (file_)
    {
        fclose(file_);
    }
}

void CacheFile::append(const char *data, size_t length)
{
    if (file_)
        fwrite(data, length, 1, file_);
}

size_t CacheFile::length()
{
    if (file_)
        return ftell(file_);
    return 0;
}

char *CacheFile::data()
{
    if (!file_)
        return nullptr;
    if (!data_)
    {
        fflush(file_);
        auto fd = fileno(file_);
        dataLength_ = length();
        data_ = static_cast<char *>(
            mmap(nullptr, dataLength_, PROT_READ, MAP_SHARED, fd, 0));
        if (data_ == MAP_FAILED)
        {
            data_ = nullptr;
            LOG_SYSERR << "mmap:";
        }
    }
    return data_;
}