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
const std::map<std::string, std::string> &MultiPartParser::getParameters() const
{
    return parameters_;
}

int MultiPartParser::parse(const HttpRequestPtr &req)
{
    if (req->method() != Post && req->method() != Put)
        return -1;
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
    return parse(req,
                 contentType.data() + (pos + 9),
                 contentType.size() - (pos + 9));
}
static std::pair<string_view, string_view> parseLine(const char *begin,
                                                     const char *end)
{
    auto p = begin;
    while (p != end)
    {
        if (*p == ':')
        {
            if (p + 1 != end && *(p + 1) == ' ')
            {
                return std::make_pair(string_view(begin, p - begin),
                                      string_view(p + 2, end - p - 2));
            }
            else
            {
                return std::make_pair(string_view(begin, p - begin),
                                      string_view(p + 1, end - p - 1));
            }
        }
        ++p;
    }
    return std::make_pair(string_view(), string_view());
}
int MultiPartParser::parseEntity(const char *begin, const char *end)
{
    static const char entityName[] = "name=";
    static const char semiColon[] = ";";
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
            auto namePos = std::search(value.begin(),
                                       value.end(),
                                       entityName,
                                       entityName + 5);
            if (namePos == value.end())
            {
                return -1;
            }
            namePos += 5;
            auto semiColonPos =
                std::search(namePos, value.end(), semiColon, semiColon + 1);
            std::string name{*namePos == '"' ? namePos + 1 : namePos,
                             *(semiColonPos - 1) == '"' ? semiColonPos - 1
                                                        : semiColonPos};
            auto fileNamePos =
                std::search(semiColonPos, value.end(), fileName, fileName + 9);
            if (fileNamePos == value.end())
            {
                parameters_.emplace(name, std::string(headEnd + 2, end));
                return 0;
            }
            else
            {
                fileNamePos += 9;
                auto semiColonPos = std::search(fileNamePos,
                                                value.end(),
                                                semiColon,
                                                semiColon + 1);
                std::string fileName{*fileNamePos == '"' ? fileNamePos + 1
                                                         : fileNamePos,
                                     *(semiColonPos - 1) == '"'
                                         ? semiColonPos - 1
                                         : semiColonPos};

                filePtr->setRequest(requestPtr_);
                filePtr->setItemName(std::move(name));
                filePtr->setFileName(std::move(fileName));
                filePtr->setFile(headEnd + 2,
                                 static_cast<size_t>(end - headEnd - 2));
            }
        }
        else if (key == "content-type")
        {
            auto value = keyAndValue.second;
            auto semiColonPos = std::search(value.begin(),
                                            value.end(),
                                            semiColon,
                                            semiColon + 1);
            filePtr->setContentType(parseContentType(
                string_view(value.begin(), semiColonPos - value.begin())));
        }
        else if (key == "content-transfer-encoding")
        {
            auto value = keyAndValue.second;
            auto semiColonPos = std::search(value.begin(),
                                            value.end(),
                                            semiColon,
                                            semiColon + 1);

            filePtr->setContentTransferEncoding(
                std::string{value.begin(), semiColonPos});
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
    string_view boundary{boundaryData, boundaryLen};
    if (boundary.size() > 2 && boundary[0] == '\"')
        boundary = boundary.substr(1, boundary.size() - 2);
    requestPtr_ = req;
    string_view::size_type pos1, pos2;
    pos1 = 0;
    auto content = static_cast<HttpRequestImpl *>(req.get())->bodyView();
    pos2 = content.find(boundary);
    while (1)
    {
        pos1 = pos2;
        if (pos1 == string_view::npos)
            break;
        pos1 += boundary.length();
        if (content[pos1] == '\r' && content[pos1 + 1] == '\n')
            pos1 += 2;
        pos2 = content.find(boundary, pos1);
        if (pos2 == string_view::npos)
            break;
        //    std::cout<<"pos1="<<pos1<<" pos2="<<pos2<<std::endl;
        if (content[pos2 - 4] == '\r' && content[pos2 - 3] == '\n' &&
            content[pos2 - 2] == '-' && content[pos2 - 1] == '-')
            pos2 -= 4;
        if (parseEntity(content.data() + pos1, content.data() + pos2) != 0)
            return -1;
        // pos2+=boundary.length();
    }
    return 0;
}
