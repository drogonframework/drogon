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
#include <string_view>
#include <stdio.h>

namespace drogon
{
class CacheFile : public trantor::NonCopyable
{
  public:
    explicit CacheFile(const std::string &path, bool autoDelete = true);
    ~CacheFile();

    void append(const std::string &data)
    {
        append(data.data(), data.length());
    }

    void append(const char *data, size_t length);

    std::string_view getStringView()
    {
        if (data())
            return std::string_view(data_, dataLength_);
        return std::string_view();
    }

  private:
    char *data();
    size_t length();
    FILE *file_{nullptr};
    bool autoDelete_{true};
    const std::string path_;
    char *data_{nullptr};
    size_t dataLength_{0};
};
}  // namespace drogon
