/**
 *
 *  FileUpload.cc
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
#include <drogon/utils/Utilities.h>
#include <drogon/FileUpload.h>
#include <drogon/HttpAppFramework.h>
#ifdef USE_OPENSSL
#include <openssl/md5.h>
#else
#include "ssl_funcs/Md5.h"
#endif
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

using namespace drogon;

const std::vector<HttpFile> &FileUpload::getFiles()
{
    return _files;
}
const std::map<std::string, std::string> &FileUpload::getParameters() const
{
    return _parameters;
};
int FileUpload::parse(const HttpRequestPtr &req)
{
    if (req->method() != Post)
        return -1;
    const std::string &contentType = std::dynamic_pointer_cast<HttpRequestImpl>(req)->getHeaderBy("content-type");
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
    //std::cout << "boundary[" << boundary << "]" << std::endl;
    std::string content = req->query();
    std::string::size_type pos1, pos2;
    pos1 = 0;
    pos2 = content.find(boundary);
    while (1)
    {
        pos1 = pos2;
        if (pos1 == std::string::npos)
            break;
        pos1 += boundary.length();
        if (content[pos1] == '\r' && content[pos1 + 1] == '\n')
            pos1 += 2;
        pos2 = content.find(boundary, pos1);
        if (pos2 == std::string::npos)
            break;
        //    std::cout<<"pos1="<<pos1<<" pos2="<<pos2<<std::endl;
        if (content[pos2 - 4] == '\r' && content[pos2 - 3] == '\n' && content[pos2 - 2] == '-' && content[pos2 - 1] == '-')
            pos2 -= 4;
        if (parseEntity(content.substr(pos1, pos2 - pos1)) != 0)
            return -1;
        //pos2+=boundary.length();
    }
    return 0;
}
int FileUpload::parseEntity(const std::string &str)
{
    //    parseEntity:[Content-Disposition: form-data; name="userfile1"; filename="appID.txt"
    //    Content-Type: text/plain
    //
    //    AA004MV7QI]
    //    pos1=190 pos2=251
    //    parseEntity:[YvBu
    //    Content-Disposition: form-data; name="text"
    //
    //    text]

    // std::cout<<"parseEntity:["<<str<<"]"<<std::endl;
    std::string::size_type pos = str.find("name=\"");
    if (pos == std::string::npos)
        return -1;
    pos += 6;
    std::string::size_type pos1 = str.find("\"", pos);
    if (pos1 == std::string::npos)
        return -1;
    std::string name = str.substr(pos, pos1 - pos);
    // std::cout<<"name=["<<name<<"]\n";
    pos = str.find("filename=\"");
    if (pos == std::string::npos)
    {
        pos1 = str.find("\r\n\r\n");
        if (pos1 == std::string::npos)
            return -1;
        _parameters[name] = str.substr(pos1 + 4);
        return 0;
    }
    else
    {
        pos += 10;
        std::string::size_type pos1 = str.find("\"", pos);
        if (pos1 == std::string::npos)
            return -1;
        HttpFile file;
        file.setFileName(str.substr(pos, pos1 - pos));
        pos1 = str.find("\r\n\r\n");
        if (pos1 == std::string::npos)
            return -1;
        file.setFile(str.substr(pos1 + 4));
        _files.push_back(file);
        return 0;
    }
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
        (path.length() >= 3 && path[0] == '.' && path[1] == '.' && path[2] == '/') ||
        path == "." || path == "..")
    {
        //Absolute or relative path	    
    }
    else
    {
        auto &uploadPath = drogon::app().getUploadPath();
        if (uploadPath[uploadPath.length() - 1] == '/')
            tmpPath = uploadPath + path;
        else
            tmpPath = uploadPath + "/" + path;
    }

    if (createPath(tmpPath) < 0)
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
        (filename.length() >= 3 && filename[0] == '.' && filename[1] == '.' && filename[2] == '/'))
    {
        //Absolute or relative path	    
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
        if (createPath(path) < 0)
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
#ifdef USE_OPENSSL
    MD5_CTX c;
    unsigned char md5[16] = {0};
    MD5_Init(&c);
    MD5_Update(&c, _fileContent.c_str(), _fileContent.size());
    MD5_Final(md5, &c);
    return binaryStringToHex(md5, 16);
#else
    return Md5Encode::encode(_fileContent);
#endif
}
