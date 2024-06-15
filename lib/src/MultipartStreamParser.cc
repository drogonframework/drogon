/**
 *
 *  @file MultipartStreamParser.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "MultipartStreamParser.h"
#include <cassert>

using namespace drogon;

static bool startsWith(const std::string_view &a, const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

static bool startsWithIgnoreCase(const std::string_view &a,
                                 const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (::tolower(a[i]) != ::tolower(b[i]))
        {
            return false;
        }
    }
    return true;
}

MultipartStreamParser::MultipartStreamParser(const std::string &contentType)
{
    static const std::string_view multipart = "multipart/form-data";
    static const std::string_view boundaryEq = "boundary=";

    if (!startsWithIgnoreCase(contentType, multipart))
    {
        isValid_ = false;
        return;
    }
    auto pos = contentType.find(boundaryEq, multipart.size());
    if (pos == std::string::npos)
    {
        isValid_ = false;
        return;
    }

    pos += boundaryEq.size();
    size_t pos2;
    if (contentType[pos] == '"')
    {
        ++pos;
        pos2 = contentType.find('"', pos);
    }
    else
    {
        pos2 = contentType.find(';', pos);
    }
    if (pos2 == std::string::npos)
        pos2 = contentType.size();

    boundary_ = contentType.substr(pos, pos2 - pos);
    dashBoundaryCrlf_ = dash_ + boundary_ + crlf_;
    crlfDashBoundary_ = crlf_ + dash_ + boundary_;
}

// TODO: same function in HttpRequestParser.cc
static std::pair<std::string_view, std::string_view> parseLine(
    const char *begin,
    const char *end)
{
    auto p = begin;
    while (p != end)
    {
        if (*p == ':')
        {
            if (p + 1 != end && *(p + 1) == ' ')
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 2, end - p - 2));
            }
            else
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 1, end - p - 1));
            }
        }
        ++p;
    }
    return std::make_pair(std::string_view(), std::string_view());
}

void drogon::MultipartStreamParser::parse(
    const char *data,
    size_t length,
    const drogon::RequestStreamReader::MultipartHeaderCallback &headerCb,
    const drogon::RequestStreamReader::StreamDataCallback &dataCb)
{
    buffer_.append(data, length);

    while (buffer_.size() > 0)
    {
        switch (status_)
        {
            case Status::kExpectFirstBoundary:
            {
                if (buffer_.size() < dashBoundaryCrlf_.size())
                {
                    return;
                }
                std::string_view v = buffer_.view();
                auto pos = v.find(dashBoundaryCrlf_);
                // ignore everything before the first boundary
                if (pos == std::string::npos)
                {
                    buffer_.eraseFront(buffer_.size() -
                                       dashBoundaryCrlf_.size());
                    return;
                }
                // found
                buffer_.eraseFront(pos + dashBoundaryCrlf_.size());
                status_ = Status::kExpectNewEntry;
                continue;
            }
            case Status::kExpectNewEntry:
            {
                currentHeader_.name.clear();
                currentHeader_.filename.clear();
                currentHeader_.contentType.clear();
                status_ = Status::kExpectHeader;
                continue;
            }
            case Status::kExpectHeader:
            {
                std::string_view v = buffer_.view();
                auto pos = v.find(crlf_);
                if (pos == std::string::npos)
                {
                    // same magic number in HttpRequestParser::parseRequest()
                    if (buffer_.size() > 60 * 1024)
                    {
                        isValid_ = false;
                    }
                    return;  // header incomplete, wait for more data
                }
                // empty line
                if (pos == 0)
                {
                    buffer_.eraseFront(crlf_.size());
                    status_ = Status::kExpectBody;
                    headerCb(currentHeader_);
                    continue;
                }

                // found header line
                auto [keyView, valueView] = parseLine(v.data(), v.data() + pos);
                if (keyView.empty() || valueView.empty())
                {
                    // Bad header
                    isValid_ = false;
                    return;
                }

                if (startsWithIgnoreCase(keyView, "content-type"))
                {
                    currentHeader_.contentType = valueView;
                }
                else if (startsWithIgnoreCase(keyView, "content-disposition"))
                {
                    static const std::string_view nameKey = "name=";
                    static const std::string_view fileNameKey = "filename=";

                    // Extract name
                    auto namePos = valueView.find(nameKey);
                    if (namePos == std::string::npos)
                    {
                        // name absent
                        isValid_ = false;
                        return;
                    }
                    namePos += nameKey.size();
                    size_t nameEnd;
                    if (valueView[namePos] == '"')
                    {
                        ++namePos;
                        nameEnd = valueView.find('"', namePos);
                    }
                    else
                    {
                        nameEnd = valueView.find(';', namePos);
                    }
                    if (nameEnd == std::string::npos)
                    {
                        // name end not found
                        isValid_ = false;
                        return;
                    }
                    currentHeader_.name =
                        valueView.substr(namePos, nameEnd - namePos);

                    // Extract filename
                    auto fileNamePos = valueView.find(fileNameKey, nameEnd);
                    if (fileNamePos != std::string::npos)
                    {
                        fileNamePos += fileNameKey.size();
                        size_t fileNameEnd;
                        if (valueView[fileNamePos] == '"')
                        {
                            ++fileNamePos;
                            fileNameEnd = valueView.find('"', fileNamePos);
                        }
                        else
                        {
                            fileNameEnd = valueView.find(';', fileNamePos);
                        }
                        currentHeader_.filename =
                            valueView.substr(fileNamePos,
                                             fileNameEnd - fileNamePos);
                    }
                }
                // ignore other headers
                buffer_.eraseFront(pos + crlf_.size());
                continue;
            }
            case Status::kExpectBody:
            {
                if (buffer_.size() < crlfDashBoundary_.size())
                {
                    return;  // not enough data to check boundary
                }
                std::string_view v = buffer_.view();
                auto pos = v.find(crlfDashBoundary_);
                if (pos == std::string::npos)
                {
                    // boundary not found, leave potential partial boundary
                    size_t len = v.size() - crlfDashBoundary_.size();
                    if (len > 0)
                    {
                        dataCb(v.data(), len);
                        buffer_.eraseFront(len);
                    }
                    return;
                }
                // found boundary
                dataCb(v.data(), pos);
                if (pos > 0)
                {
                    dataCb(v.data() + pos, 0);  // notify end of file
                }
                buffer_.eraseFront(pos + crlfDashBoundary_.size());
                status_ = Status::kExpectEndOrNewEntry;
                continue;
            }
            case Status::kExpectEndOrNewEntry:
            {
                std::string_view v = buffer_.view();
                // Check new entry
                if (v.size() < crlf_.size())
                {
                    return;
                }
                if (startsWith(v, crlf_))
                {
                    buffer_.eraseFront(crlf_.size());
                    status_ = Status::kExpectNewEntry;
                    continue;
                }

                // Check end
                if (v.size() < dash_.size())
                {
                    return;
                }
                if (startsWith(v, dash_))
                {
                    isFinished_ = true;
                    buffer_.clear();  // ignore epilogue
                    return;
                }
                isValid_ = false;
                return;
            }
        }
    }
}

std::string_view MultipartStreamParser::Buffer::view() const
{
    return {buffer_.data() + bufHead_, size()};
}

void MultipartStreamParser::Buffer::append(const char *data, size_t length)
{
    size_t remainSize = size();
    // Move existing data to the front
    if (remainSize > 0 && bufHead_ > 0)
    {
        for (size_t i = 0; i < remainSize; i++)
        {
            buffer_[i] = buffer_[bufHead_ + i];
        }
    }
    bufHead_ = 0;
    bufTail_ = remainSize;

    if (remainSize + length > buffer_.size())
    {
        buffer_.resize(remainSize + length);
    }

    for (size_t i = 0; i < length; ++i)
    {
        buffer_[bufTail_ + i] = data[i];
    }
    bufTail_ += length;
}

size_t MultipartStreamParser::Buffer::size() const
{
    return bufTail_ - bufHead_;
}

void MultipartStreamParser::Buffer::eraseFront(size_t length)
{
    assert(length <= size());
    bufHead_ += length;
}

void MultipartStreamParser::Buffer::clear()
{
    buffer_.clear();
    bufHead_ = 0;
    bufTail_ = 0;
}
