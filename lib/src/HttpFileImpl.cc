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
#include "filesystem.h"
#include <drogon/MultiPart.h>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace drogon;

int HttpFileImpl::save() const
{
    return save(HttpAppFrameworkImpl::instance().getUploadPath());
}

int HttpFileImpl::save(const std::string &path) const
{
    assert(!path.empty());
    if (fileName_.empty())
        return -1;
    filesystem::path fsUploadDir(utils::toNativePath(path));

    if (!fsUploadDir.is_absolute() && (!fsUploadDir.has_parent_path() ||
                                       (fsUploadDir.begin()->string() != "." &&
                                        fsUploadDir.begin()->string() != "..")))
    {
        fsUploadDir = utils::toNativePath(
                          HttpAppFrameworkImpl::instance().getUploadPath()) /
                      fsUploadDir;
    }

    fsUploadDir = filesystem::weakly_canonical(fsUploadDir);

    if (!filesystem::exists(fsUploadDir))
    {
        LOG_TRACE << "create path:" << fsUploadDir;
        drogon::error_code err;
        filesystem::create_directories(fsUploadDir, err);
        if (err)
        {
            LOG_SYSERR;
            return -1;
        }
    }

    filesystem::path fsSaveToPath(filesystem::weakly_canonical(
        fsUploadDir / utils::toNativePath(fileName_)));

    if (!std::equal(fsUploadDir.begin(),
                    fsUploadDir.end(),
                    fsSaveToPath.begin()))
    {
        LOG_ERROR
            << "Attempt writing outside of upload directory detected. Path: "
            << fileName_;
        return -1;
    }

    return saveTo(fsSaveToPath);
}
int HttpFileImpl::saveAs(const std::string &fileName) const
{
    assert(!fileName.empty());
    filesystem::path fsFileName(utils::toNativePath(fileName));
    if (!fsFileName.is_absolute() && (!fsFileName.has_parent_path() ||
                                      (fsFileName.begin()->string() != "." &&
                                       fsFileName.begin()->string() != "..")))
    {
        filesystem::path fsUploadPath(utils::toNativePath(
            HttpAppFrameworkImpl::instance().getUploadPath()));
        fsFileName = fsUploadPath / fsFileName;
    }
    if (fsFileName.has_parent_path() &&
        !filesystem::exists(fsFileName.parent_path()))
    {
        LOG_TRACE << "create path:" << fsFileName.parent_path();
        drogon::error_code err;
        filesystem::create_directories(fsFileName.parent_path(), err);
        if (err)
        {
            LOG_SYSERR;
            return -1;
        }
    }
    return saveTo(fsFileName);
}
int HttpFileImpl::saveTo(const filesystem::path &pathAndFileName) const
{
    LOG_TRACE << "save uploaded file:" << pathAndFileName;
    auto wPath = utils::toNativePath(pathAndFileName.native());
    std::ofstream file(wPath, std::ios::binary);
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
