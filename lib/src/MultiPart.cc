/**
 *
 *  @file MultiPart.cc
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

#include "HttpRequestImpl.h"
#include "HttpUtils.h"
#include "HttpAppFrameworkImpl.h"
#include "HttpFileImpl.h"
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include <drogon/config.h>
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace drogon;
using drogon::internal::parseHeaderParameter;
using drogon::internal::parseLine;
using drogon::internal::parseMultipartBoundary;

const std::vector<HttpFile> &MultiPartParser::getFiles() const
{
    return files_;
}

std::unordered_map<std::string, HttpFile> MultiPartParser::getFilesMap() const
{
    std::unordered_map<std::string, HttpFile> result;
    for (auto &file : files_)
    {
        result.emplace(file.getItemName(), file);
    }
    return result;
}

const SafeStringMap<std::string> &MultiPartParser::getParameters() const
{
    return parameters_;
}

int MultiPartParser::parse(const HttpRequestPtr &req)
{
    switch (req->method())
    {
        case Post:
        case Put:
        case Patch:
            break;
        default:
            return -1;
    }

    const std::string &contentType =
        static_cast<HttpRequestImpl *>(req.get())->getHeaderBy("content-type");
    if (contentType.empty())
    {
        return -1;
    }
    std::string_view boundary;
    if (!parseMultipartBoundary(contentType, boundary))
    {
        return -1;
    }
    return parse(req, boundary.data(), boundary.size());
}

int MultiPartParser::parseEntity(const HttpRequestPtr &req,
                                 const char *begin,
                                 const char *end)
{
    static const char CRLF[] = "\r\n\r\n";
    auto headEnd = std::search(begin, end, CRLF, CRLF + 4);
    if (headEnd == end)
    {
        return -1;
    }
    headEnd += 2;
    auto pos = begin;
    std::shared_ptr<HttpFileImpl> filePtr = std::make_shared<HttpFileImpl>();
    while (pos != headEnd)
    {
        auto lineEnd = std::search(pos, headEnd, CRLF, CRLF + 2);
        auto keyAndValue = parseLine(pos, lineEnd);
        if (keyAndValue.first.empty() || keyAndValue.second.empty())
        {
            return -1;
        }
        pos = lineEnd + 2;
        std::string key{keyAndValue.first.data(), keyAndValue.first.size()};
        std::transform(key.begin(),
                       key.end(),
                       key.begin(),
                       [](unsigned char c) { return tolower(c); });
        if (key == "content-disposition")
        {
            auto value = keyAndValue.second;
            std::string_view nameValue;
            if (!parseHeaderParameter(value, "name", nameValue))
            {
                return -1;
            }
            std::string name{nameValue};

            std::string_view fileNameValue;
            if (!parseHeaderParameter(value, "filename", fileNameValue))
            {
                parameters_.emplace(name, std::string(headEnd + 2, end));
                return 0;
            }
            else
            {
                std::string fName{fileNameValue};
                filePtr->setRequest(req);
                filePtr->setItemName(std::move(name));
                filePtr->setFileName(std::move(fName));
                filePtr->setFile(headEnd + 2,
                                 static_cast<size_t>(end - headEnd - 2));
            }
        }
        else if (key == "content-type")
        {
            filePtr->setContentType(
                parseContentType(drogon::internal::headerValueBeforeParameters(
                    keyAndValue.second)));
        }
        else if (key == "content-transfer-encoding")
        {
            filePtr->setContentTransferEncoding(
                std::string(drogon::internal::headerValueBeforeParameters(
                    keyAndValue.second)));
        }
    }
    if (!filePtr->getFileName().empty())
    {
        files_.emplace_back(std::move(filePtr));
        return 0;
    }
    else
    {
        return -1;
    }
}

int MultiPartParser::parse(const HttpRequestPtr &req,
                           const char *boundaryData,
                           size_t boundaryLen)
{
    std::string_view boundary{boundaryData, boundaryLen};
    if (boundary.empty())
        return -1;

    const std::string firstBoundary = "--" + std::string(boundary);
    const std::string delimiter = "\r\n" + firstBoundary;
    const std::string_view content =
        static_cast<HttpRequestImpl *>(req.get())->bodyView();
    std::string_view remain = content;
    std::string_view part;

    // The first delimiter starts at the beginning of the body or after a
    // preamble line. A bare occurrence of the boundary is not a delimiter.
    while (true)
    {
        const auto pos = remain.find(firstBoundary);
        if (pos == std::string_view::npos)
        {
            return -1;
        }
        const auto offset = content.size() - remain.size() + pos;
        const auto suffix = remain.substr(pos + firstBoundary.size());
        if ((offset == 0 ||
             (offset >= 2 && content.substr(offset - 2, 2) == "\r\n")) &&
            (internal::startsWith(suffix, "\r\n") ||
             internal::startsWith(suffix, "--")))
        {
            if (internal::startsWith(suffix, "--"))
            {
                return 0;  // empty multipart body
            }
            part = suffix.substr(2);
            break;
        }
        remain = remain.substr(pos + 1);
    }

    while (true)
    {
        remain = part;
        while (true)
        {
            const auto pos = remain.find(delimiter);
            if (pos == std::string_view::npos)
            {
                return -1;
            }
            const auto suffix = remain.substr(pos + delimiter.size());
            if (internal::startsWith(suffix, "\r\n") ||
                internal::startsWith(suffix, "--"))
            {
                const auto length = part.size() - remain.size() + pos;
                if (parseEntity(req, part.data(), part.data() + length) != 0)
                {
                    return -1;
                }
                if (internal::startsWith(suffix, "--"))
                {
                    return 0;
                }
                part = suffix.substr(2);
                break;
            }
            remain = remain.substr(pos + 1);
        }
    }
}
