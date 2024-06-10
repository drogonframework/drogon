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
    std::string::size_type pos = contentType.find(';');
    if (pos == std::string::npos)
        return -1;

    std::string type = contentType.substr(0, pos);
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return tolower(c);
    });
    if (type != "multipart/form-data")
        return -1;
    pos = contentType.find("boundary=");
    if (pos == std::string::npos)
        return -1;
    auto pos2 = contentType.find(';', pos);
    if (pos2 == std::string::npos)
        pos2 = contentType.size();
    return parse(req, contentType.data() + (pos + 9), pos2 - (pos + 9));
}

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

int MultiPartParser::parseEntity(const HttpRequestPtr &req,
                                 const char *begin,
                                 const char *end)
{
    static const char entityName[] = "name=";
    static const char fileName[] = "filename=";
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
            auto valueEnd = value.data() + value.length();
            auto namePos =
                std::search(value.data(), valueEnd, entityName, entityName + 5);
            if (namePos == valueEnd)
            {
                return -1;
            }
            namePos += 5;
            const char *nameEnd;
            if (*namePos == '"')
            {
                ++namePos;
                nameEnd = std::find(namePos, valueEnd, '"');
            }
            else
            {
                nameEnd = std::find(namePos, valueEnd, ';');
            }
            std::string name(namePos, nameEnd);
            auto fileNamePos =
                std::search(nameEnd, valueEnd, fileName, fileName + 9);
            if (fileNamePos == valueEnd)
            {
                parameters_.emplace(name, std::string(headEnd + 2, end));
                return 0;
            }
            else
            {
                fileNamePos += 9;
                const char *fileNameEnd;
                if (*fileNamePos == '"')
                {
                    ++fileNamePos;
                    fileNameEnd = std::find(fileNamePos, valueEnd, '"');
                }
                else
                {
                    fileNameEnd = std::find(fileNamePos, valueEnd, ';');
                }
                std::string fName{fileNamePos, fileNameEnd};
                filePtr->setRequest(req);
                filePtr->setItemName(std::move(name));
                filePtr->setFileName(std::move(fName));
                filePtr->setFile(headEnd + 2,
                                 static_cast<size_t>(end - headEnd - 2));
            }
        }
        else if (key == "content-type")
        {
            auto value = keyAndValue.second;
            auto semiColonPos =
                std::find(value.data(), value.data() + value.length(), ';');
            std::string_view contentType(value.data(),
                                         semiColonPos - value.data());
            filePtr->setContentType(parseContentType(contentType));
        }
        else if (key == "content-transfer-encoding")
        {
            auto value = keyAndValue.second;
            auto semiColonPos =
                std::find(value.data(), value.data() + value.length(), ';');

            filePtr->setContentTransferEncoding(
                std::string{value.data(), semiColonPos});
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
    if (boundary.size() > 2 && boundary[0] == '\"')
        boundary = boundary.substr(1, boundary.size() - 2);
    std::string_view::size_type pos1, pos2;
    pos1 = 0;
    auto content = static_cast<HttpRequestImpl *>(req.get())->bodyView();
    pos2 = content.find(boundary);
    while (true)
    {
        pos1 = pos2;
        if (pos1 == std::string_view::npos)
            break;
        pos1 += boundary.length();
        if (content[pos1] == '\r' && content[pos1 + 1] == '\n')
            pos1 += 2;
        pos2 = content.find(boundary, pos1);
        if (pos2 == std::string_view::npos)
            break;
        bool flag = false;
        if (content[pos2 - 4] == '\r' && content[pos2 - 3] == '\n' &&
            content[pos2 - 2] == '-' && content[pos2 - 1] == '-')
        {
            pos2 -= 4;
            flag = true;
        }
        if (parseEntity(req, content.data() + pos1, content.data() + pos2) != 0)
            return -1;
        if (flag)
            pos2 += 4;
    }
    return 0;
}
