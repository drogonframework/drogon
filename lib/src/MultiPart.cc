/**
 *
 *  MultiPart.cc
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

#include "HttpRequestImpl.h"
#include "HttpUtils.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include <drogon/config.h>
#ifdef OpenSSL_FOUND
#include <openssl/md5.h>
#else
#include "ssl_funcs/Md5.h"
#endif
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace drogon;

const std::vector<HttpFile> &MultiPartParser::getFiles()
{
    return _files;
}

const std::map<std::string, std::string> &MultiPartParser::getParameters() const
{
    return _parameters;
}

int MultiPartParser::parse(const HttpRequestPtr &req)
{
    if (req->method() != Post)
        return -1;
    const std::string &contentType =
        static_cast<HttpRequestImpl *>(req.get())->getHeaderBy(
            "content-type");
    if (contentType.empty())
    {
        return -1;
    }
    std::string::size_type pos = contentType.find(";");
    if (pos == std::string::npos)
        return -1;

    std::string type = contentType.substr(0, pos);
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (type != "multipart/form-data")
        return -1;
    pos = contentType.find("boundary=");
    if (pos == std::string::npos)
        return -1;
    std::string boundary = contentType.substr(pos + 9);

    return parse(req, boundary);
}

int MultiPartParser::parseEntity(const char *begin, const char *end)
{
    static const char entityName[] = "name=\"";
    static const char quotationMark[] = "\"";
    static const char fileName[] = "filename=\"";
    static const char CRLF[] = "\r\n\r\n";

    auto pos = std::search(begin, end, entityName, entityName + 6);
    if (pos == end)
        return -1;
    pos += 6;
    auto pos1 = std::search(pos, end, quotationMark, quotationMark + 1);
    if (pos1 == end)
        return -1;
    std::string name(pos, pos1);
    pos = std::search(pos1, end, fileName, fileName + 10);
    if (pos == end)
    {
        pos1 = std::search(pos1, end, CRLF, CRLF + 4);
        if (pos1 == end)
            return -1;
        _parameters[name] = std::string(pos1 + 4, end);
        return 0;
    }
    else
    {
        pos += 10;
        auto pos1 = std::search(pos, end, quotationMark, quotationMark + 1);
        if (pos1 == end)
            return -1;
        HttpFile file;
        file.setFileName(std::string(pos, pos1));
        pos1 = std::search(pos1, end, CRLF, CRLF + 4);
        if (pos1 == end)
            return -1;
        file.setFile(std::string(pos1 + 4, end));
        _files.push_back(std::move(file));
        return 0;
    }
}

int MultiPartParser::parse(const HttpRequestPtr &req,
                           const std::string &boundary)
{
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

int HttpFile::save(const std::string &path) const
{
    assert(!path.empty());
    if (_fileName == "")
        return -1;
    std::string filename;
    auto tmpPath = path;
    if (path[0] == '/' ||
        (path.length() >= 2 && path[0] == '.' && path[1] == '/') ||
        (path.length() >= 3 && path[0] == '.' && path[1] == '.' &&
         path[2] == '/') ||
        path == "." || path == "..")
    {
        // Absolute or relative path
    }
    else
    {
        auto &uploadPath = drogon::app().getUploadPath();
        if (uploadPath[uploadPath.length() - 1] == '/')
            tmpPath = uploadPath + path;
        else
            tmpPath = uploadPath + "/" + path;
    }

    if (utils::createPath(tmpPath) < 0)
        return -1;

    if (tmpPath[tmpPath.length() - 1] != '/')
    {
        filename = tmpPath + "/" + _fileName;
    }
    else
        filename = tmpPath + _fileName;

    return saveTo(filename);
}
int HttpFile::save() const
{
    return save(drogon::app().getUploadPath());
}
int HttpFile::saveAs(const std::string &filename) const
{
    assert(!filename.empty());
    auto pathAndFileName = filename;
    if (filename[0] == '/' ||
        (filename.length() >= 2 && filename[0] == '.' && filename[1] == '/') ||
        (filename.length() >= 3 && filename[0] == '.' && filename[1] == '.' &&
         filename[2] == '/'))
    {
        // Absolute or relative path
    }
    else
    {
        auto &uploadPath = drogon::app().getUploadPath();
        if (uploadPath[uploadPath.length() - 1] == '/')
            pathAndFileName = uploadPath + filename;
        else
            pathAndFileName = uploadPath + "/" + filename;
    }
    auto pathPos = pathAndFileName.rfind("/");
    if (pathPos != std::string::npos)
    {
        std::string path = pathAndFileName.substr(0, pathPos);
        if (utils::createPath(path) < 0)
            return -1;
    }
    return saveTo(pathAndFileName);
}
int HttpFile::saveTo(const std::string &pathAndFilename) const
{
    LOG_TRACE << "save uploaded file:" << pathAndFilename;
    std::ofstream file(pathAndFilename);
    if (file.is_open())
    {
        file << _fileContent;
        file.close();
        return 0;
    }
    else
    {
        LOG_ERROR << "save failed!";
        return -1;
    }
}
const std::string HttpFile::getMd5() const
{
#ifdef OpenSSL_FOUND
    MD5_CTX c;
    unsigned char md5[16] = {0};
    MD5_Init(&c);
    MD5_Update(&c, _fileContent.c_str(), _fileContent.size());
    MD5_Final(md5, &c);
    return utils::binaryStringToHex(md5, 16);
#else
    return Md5Encode::encode(_fileContent);
#endif
}
