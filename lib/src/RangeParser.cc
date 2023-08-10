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

#include "RangeParser.h"

#include <limits>

using namespace drogon;

static constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max();
static constexpr size_t MAX_TEN = MAX_SIZE / 10;
static constexpr size_t MAX_DIGIT = MAX_SIZE % 10;

// clang-format off
#define DR_SKIP_WHITESPACE(p) while (*p == ' ') { ++(p); }
#define DR_ISDIGIT(p) ('0' <= *(p) && *(p) <= '9')
#define DR_WOULD_OVERFLOW(base, digit) \
    (static_cast<size_t>(base) > MAX_TEN || \
    (static_cast<size_t>(base) >= MAX_TEN && \
    static_cast<size_t>(digit) - '0' > MAX_DIGIT))

// clang-format on

/** Following formats are valid range header according to rfc7233`
 * Range: <unit>=<start>-
 * Range: <unit>=<start>-<end>
 * Range: <unit>=<start>-<end>, <start>-<end>
 * Range: <unit>=<start>-<end>, <start>-<end>, <start>-<end>
 * Range: <unit>=-<suffix-length>
 */

FileRangeParseResult drogon::parseRangeHeader(const std::string &rangeStr,
                                              size_t contentLength,
                                              std::vector<FileRange> &ranges)
{
    if (rangeStr.size() < 7 || rangeStr.compare(0, 6, "bytes=") != 0)
    {
        return InvalidRange;
    }
    const char *iter = rangeStr.c_str() + 6;

    size_t totalSize = 0;
    while (true)
    {
        size_t start = 0;
        size_t end = 0;
        // If this is a suffix range: <unit>=-<suffix-length>
        bool isSuffix = false;

        DR_SKIP_WHITESPACE(iter);

        if (*iter == '-')
        {
            isSuffix = true;
            ++iter;
        }
        // Parse start
        else
        {
            if (!DR_ISDIGIT(iter))
            {
                return InvalidRange;
            }
            while (DR_ISDIGIT(iter))
            {
                // integer out of range
                if (DR_WOULD_OVERFLOW(start, *iter))
                {
                    return NotSatisfiable;
                }
                start = start * 10 + (*iter++ - '0');
            }
            DR_SKIP_WHITESPACE(iter);
            // should be separator now
            if (*iter++ != '-')
            {
                return InvalidRange;
            }
            DR_SKIP_WHITESPACE(iter);
            // If this is a prefix range <unit>=<range-start>-
            if (*iter == ',' || *iter == '\0')
            {
                end = contentLength;
                // Handle found
                if (start < end)
                {
                    if (totalSize > MAX_SIZE - (end - start))
                    {
                        return NotSatisfiable;
                    }
                    totalSize += end - start;
                    ranges.push_back({start, end});
                }
                if (*iter++ != ',')
                {
                    break;
                }
                continue;
            }
        }

        // Parse end
        if (!DR_ISDIGIT(iter))
        {
            return InvalidRange;
        }
        while (DR_ISDIGIT(iter))
        {
            if (DR_WOULD_OVERFLOW(end, *iter))
            {
                return NotSatisfiable;
            }
            end = end * 10 + (*iter++ - '0');
        }
        DR_SKIP_WHITESPACE(iter);

        if (*iter != ',' && *iter != '\0')
        {
            return InvalidRange;
        }
        if (isSuffix)
        {
            start = (end < contentLength) ? contentLength - end : 0;
            end = contentLength - 1;
        }
        // [start, end)
        if (end >= contentLength)
        {
            end = contentLength;
        }
        else
        {
            ++end;
        }

        // handle found
        if (start < end)
        {
            ranges.push_back({start, end});
            if (totalSize > MAX_SIZE - (end - start))
            {
                return NotSatisfiable;
            }
            totalSize += end - start;
            // We restrict the number to be under 100, to avoid malicious
            // requests.
            // Though rfc does not say anything about max number of ranges,
            // it does mention that server can ignore range header freely.
            if (ranges.size() > 100)
            {
                return InvalidRange;
            }
        }
        if (*iter++ != ',')
        {
            break;
        }
    }

    if (ranges.size() == 0 || totalSize > contentLength)
    {
        return NotSatisfiable;
    }
    return ranges.size() == 1 ? SinglePart : MultiPart;
}

#undef DR_SKIP_WHITESPACE
#undef DR_ISDIGIT
#undef DR_WOULD_OVERFLOW
