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
    : _autoDelete(autoDelete), _path(path)
{
    _file = fopen(_path.data(), "w+");
}

CacheFile::~CacheFile()
{
    if (_data)
    {
        munmap(_data, _dataLength);
    }
    if (_autoDelete && _file)
    {
        fclose(_file);
        unlink(_path.data());
    }
    else if (_file)
    {
        fclose(_file);
    }
}

void CacheFile::append(const char *data, size_t length)
{
    if (_file)
        fwrite(data, length, 1, _file);
}

size_t CacheFile::length()
{
    if (_file)
        return ftell(_file);
    return 0;
}

char *CacheFile::data()
{
    if (!_file)
        return nullptr;
    if (!_data)
    {
        auto fd = fileno(_file);
        _dataLength = length();
        _data = mmap(nullptr, _dataLength, PROT_READ, MAP_SHARED, fd, 0);
        if (_data == MAP_FAILED)
        {
            _data = nullptr;
            LOG_SYSERR << "mmap:";
        }
    }
    return static_cast<char *>(_data);
}