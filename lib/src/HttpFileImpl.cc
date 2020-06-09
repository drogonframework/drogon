/**
 *
 *  HttpFileImpl.cc
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

#include "HttpFileImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/MultiPart.h>
#include <fstream>
#include <iostream>

using namespace drogon;

int HttpFileImpl::save(const std::string &path) const
{
    assert(!path.empty());
    if (fileName_ == "")
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
        auto &uploadPath = HttpAppFrameworkImpl::instance().getUploadPath();
        if (uploadPath[uploadPath.length() - 1] == '/')
            tmpPath = uploadPath + path;
        else
            tmpPath = uploadPath + "/" + path;
    }

    if (utils::createPath(tmpPath) < 0)
        return -1;

    if (tmpPath[tmpPath.length() - 1] != '/')
    {
        filename = tmpPath + "/";
        filename.append(fileName_.data(), fileName_.length());
    }
    else
        filename = tmpPath.append(fileName_.data(), fileName_.length());

    return saveTo(filename);
}
int HttpFileImpl::save() const
{
    return save(HttpAppFrameworkImpl::instance().getUploadPath());
}
int HttpFileImpl::saveAs(const std::string &filename) const
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
        auto &uploadPath = HttpAppFrameworkImpl::instance().getUploadPath();
        if (uploadPath[uploadPath.length() - 1] == '/')
            pathAndFileName = uploadPath + filename;
        else
            pathAndFileName = uploadPath + "/" + filename;
    }
    auto pathPos = pathAndFileName.rfind('/');
    if (pathPos != std::string::npos)
    {
        std::string path = pathAndFileName.substr(0, pathPos);
        if (utils::createPath(path) < 0)
            return -1;
    }
    return saveTo(pathAndFileName);
}
int HttpFileImpl::saveTo(const std::string &pathAndFilename) const
{
    LOG_TRACE << "save uploaded file:" << pathAndFilename;
    std::ofstream file(pathAndFilename);
    if (file.is_open())
    {
        file.write(fileContent_.data(), fileContent_.size());
        file.close();
        return 0;
    }
    else
    {
        LOG_ERROR << "save failed!";
        return -1;
    }
}
std::string HttpFileImpl::getMd5() const
{
    return utils::getMd5(fileContent_.data(), fileContent_.size());
}

const std::string &HttpFile::getFileName() const
{
    return implPtr_->getFileName();
}

void HttpFile::setFileName(const std::string &filename)
{
    implPtr_->setFileName(filename);
}

void HttpFile::setFile(const char *data, size_t length)
{
    implPtr_->setFile(data, length);
}

int HttpFile::save() const
{
    return implPtr_->save();
}

int HttpFile::save(const std::string &path) const
{
    return implPtr_->save(path);
}

int HttpFile::saveAs(const std::string &filename) const
{
    return implPtr_->saveAs(filename);
}

size_t HttpFile::fileLength() const noexcept
{
    return implPtr_->fileLength();
}

const char *HttpFile::fileData() const noexcept
{
    return implPtr_->fileData();
}

std::string HttpFile::getMd5() const
{
    return implPtr_->getMd5();
}

HttpFile::HttpFile(std::shared_ptr<HttpFileImpl> &&implPtr)
    : implPtr_(std::move(implPtr))
{
}
