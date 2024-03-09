/**
 *
 *  RangeParser.h
 *  He, Wanchen
 *
 *  Copyright 2021, He,Wanchen.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace drogon
{
// [start, end)
struct FileRange
{
    size_t start;
    size_t end;
};

enum FileRangeParseResult
{
    InvalidRange = -1,
    NotSatisfiable = 0,
    SinglePart = 1,
    MultiPart = 2
};

FileRangeParseResult parseRangeHeader(const std::string &rangeStr,
                                      size_t contentLength,
                                      std::vector<FileRange> &ranges);

}  // namespace drogon
