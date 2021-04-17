/**
 *
 *  @file HttpFileImpl.cc
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

#include "HttpFileImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/MultiPart.h>
#include <fstream>
#include <iostream>

using namespace drogon;
// Verify if last char of path is a slash, otherwise, add the slash
static inline void ensureSlashPostfix(std::string &path)
{
    if (path[path.length() - 1] != '/')
        path += '/';
}

int HttpFileImpl::save() const
{
    return save(HttpAppFrameworkImpl::instance().getUploadPath());
}

int HttpFileImpl::save(const std::string &path) const
{
    assert(!path.empty());
    if (fileName_.empty())
        return -1;
    std::string fileName;
    if (path[0] == '/' ||
        (path.length() >= 2 && path[0] == '.' && path[1] == '/') ||
        (path.length() >= 3 && path[0] == '.' && path[1] == '.' &&
         path[2] == '/') ||
        path == "." || path == "..")
    {
        // Absolute or relative path
        fileName = path;
    }
    else
    {
        fileName = HttpAppFrameworkImpl::instance().getUploadPath();
        ensureSlashPostfix(fileName);
        fileName += path;
    }
    if (utils::createPath(fileName) < 0)
        return -1;
    ensureSlashPostfix(fileName);
    fileName += fileName_;
    return saveTo(fileName);
}
int HttpFileImpl::saveAs(const std::string &fileName) const
{
    assert(!fileName.empty());
    std::string pathAndFileName;
    if (fileName[0] == '/' ||
        (fileName.length() >= 2 && fileName[0] == '.' && fileName[1] == '/') ||
        (fileName.length() >= 3 && fileName[0] == '.' && fileName[1] == '.' &&
         fileName[2] == '/'))
    {
        // Absolute or relative path
        pathAndFileName = fileName;
    }
    else
    {
        pathAndFileName = HttpAppFrameworkImpl::instance().getUploadPath();
        ensureSlashPostfix(pathAndFileName);
        pathAndFileName += fileName;
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
int HttpFileImpl::saveTo(const std::string &pathAndFileName) const
{
    LOG_TRACE << "save uploaded file:" << pathAndFileName;
    std::ofstream file(pathAndFileName, std::ios::binary);
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

void HttpFile::setFileName(const std::string &fileName)
{
    implPtr_->setFileName(fileName);
}

string_view HttpFile::getFileExtension() const
{
    return implPtr_->getFileExtension();
}

FileType HttpFile::getFileType() const
{
    return implPtr_->getFileType();
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

int HttpFile::saveAs(const std::string &fileName) const
{
    return implPtr_->saveAs(fileName);
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

const std::string &HttpFile::getItemName() const
{
    return implPtr_->getItemName();
}
