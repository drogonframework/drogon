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
#include "HttpUtils.h"
#include <cassert>

using namespace drogon;
using drogon::internal::parseHeaderParameter;
using drogon::internal::parseLine;
using drogon::internal::parseMultipartBoundary;
using drogon::internal::startsWith;
using drogon::internal::startsWithIgnoreCase;

MultipartStreamParser::MultipartStreamParser(const std::string &contentType)
{
    std::string_view boundary;
    if (!parseMultipartBoundary(contentType, boundary))
    {
        isValid_ = false;
        return;
    }
    boundary_ = boundary;
    dashBoundaryCrlf_ = dash_ + boundary_ + crlf_;
    crlfDashBoundary_ = crlf_ + dash_ + boundary_;
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
                    std::string_view name;
                    if (!parseHeaderParameter(valueView, "name", name))
                    {
                        // name absent
                        isValid_ = false;
                        return;
                    }
                    currentHeader_.name = name;

                    // Extract filename
                    std::string_view fileName;
                    if (parseHeaderParameter(valueView, "filename", fileName))
                    {
                        currentHeader_.filename = fileName;
                    }
                }
                // ignore other headers
                buffer_.eraseFront(pos + crlf_.size());
                continue;
            }
            case Status::kExpectBody:
            {
                std::string_view v = buffer_.view();
                std::string_view remain = v;
                while (true)
                {
                    auto pos = remain.find(crlfDashBoundary_);
                    if (pos == std::string_view::npos)
                    {
                        // Retain enough bytes for a delimiter split across
                        // calls.
                        const size_t keep = crlfDashBoundary_.size() + 1;
                        if (v.size() > keep)
                        {
                            const size_t len = v.size() - keep;
                            dataCb(v.data(), len);
                            buffer_.eraseFront(len);
                        }
                        return;
                    }
                    const auto body =
                        v.substr(0, v.size() - remain.size() + pos);
                    const auto suffix =
                        remain.substr(pos + crlfDashBoundary_.size());
                    if (suffix.size() < 2)
                    {
                        // The delimiter suffix may arrive in the next call.
                        if (!body.empty())
                        {
                            dataCb(body.data(), body.size());
                            buffer_.eraseFront(body.size());
                        }
                        return;
                    }
                    if (!startsWith(suffix, crlf_) &&
                        !startsWith(suffix, dash_))
                    {
                        // A boundary prefix inside the part is ordinary data.
                        remain.remove_prefix(pos + 1);
                        continue;
                    }
                    dataCb(body.data(), body.size());
                    if (!body.empty())
                    {
                        dataCb(body.data() + body.size(), 0);
                    }
                    buffer_.eraseFront(body.size() + crlfDashBoundary_.size());
                    status_ = Status::kExpectEndOrNewEntry;
                    break;
                }
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
