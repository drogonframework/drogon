/**
 *
 *  CacheFile.h
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

#include <trantor/utils/NonCopyable.h>
#include <string>
#include <stdio.h>

namespace drogon
{
class CacheFile : public trantor::NonCopyable
{
  public:
    explicit CacheFile(const std::string &path, bool autoDelete = true);
    ~CacheFile();
    char *data();
    size_t length();
    void append(const std::string &data)
    {
        append(data.data(), data.length());
    }
    void append(const char *data, size_t length);

  private:
    FILE *_file = nullptr;
    bool _autoDelete = true;
    const std::string _path;
    void *_data = nullptr;
    size_t _dataLength = 0;
};
}  // namespace drogon